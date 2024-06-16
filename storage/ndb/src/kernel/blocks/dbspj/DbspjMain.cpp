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

#define DBSPJ_C
#include "Dbspj.hpp"

#include <ndb_version.h>
#include <AttributeDescriptor.hpp>
#include <AttributeHeader.hpp>
#include <Interpreter.hpp>
#include <KeyDescriptor.hpp>
#include <SectionReader.hpp>
#include <cstring>
#include <md5_hash.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/DbspjErr.hpp>
#include <signaldata/DiGetNodes.hpp>
#include <signaldata/DihScanTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/LqhKey.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/QueryTree.hpp>
#include <signaldata/RouteOrd.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/TcKeyRef.hpp>
#include <signaldata/TransIdAI.hpp>

#include <Bitmask.hpp>
#include <EventLogger.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/SignalDroppedRep.hpp>

#define JAM_FILE_ID 479

extern Uint32 ErrorSignalReceive;
extern Uint32 ErrorMaxSegmentsToSeize;

/**
 * 12 bits are used to represent the 'parent-row-correlation-id'.
 * Effectively limiting max rows in a batch. Also used to limit
 * the 'capacity' of the 'mapped' RowCollection
 */
static constexpr Uint32 MaxCorrelationId = (1 << 12);

/**
 * Limits for job-buffer congestion control, counted in number of
 * outstanding signal in a SPJ-request:
 *
 * If 'HighlyCongestedLimit' is reached, we start to defer LQHKEYREQ signals
 * by buffering the *parent-row* of the LQHKEYREQ, and storing its
 * correlation-id. (The LQHKEYREQ is not sent yet)
 *
 * When the reach 'MildlyCongestedLimit' we start resuming deferred
 * LQHKEYREQ signals by iterating the stored correlation-id's, locate the
 * related buffered row, which is then used to produce LQHKEYREQ's.
 * (See ::resumeCongestedNode())
 *
 * We will resume maximum 'ResumeCongestedQuota' LQHKEYREQ signals at
 * once. This serves both as a realtime break for the scheduler, and as a
 * mechanism for keeping the congestion level at ~MildlyCongestedLimit.
 * Note that we *do not* CONTINUEB when we take a real time break - Instead
 * we wait for the congestion level to reach 'MildlyCongestedLimit' again, which
 * will resume more LQHKEYREQ's. The intention of this is to leave some slack
 * for further incoming parent-rows to immediately create LQHKEYREQ's,
 * without these also having to be deferred.
 */
static constexpr Uint32 HighlyCongestedLimit = 256;
static constexpr Uint32 MildlyCongestedLimit = (HighlyCongestedLimit / 2);
static constexpr Uint32 ResumeCongestedQuota = 32;

#ifdef VM_TRACE
/**
 * DEBUG options for different parts of SPJ block.
 * Comment out those part you don't want DEBUG'ed.
 */
// #define DEBUG(x) ndbout << "DBSPJ: "<< x << endl
// #define DEBUG_DICT(x) ndbout << "DBSPJ: "<< x << endl
// #define DEBUG_LQHKEYREQ
// #define DEBUG_SCAN_FRAGREQ
#endif

/**
 * Provide empty defs for those DEBUGs which has to be defined.
 */
#if !defined(DEBUG)
#define DEBUG(x)
#endif

#if !defined(DEBUG_DICT)
#define DEBUG_DICT(x)
#endif

#define DEBUG_CRASH() ndbassert(false)

const Ptr<Dbspj::TreeNode> Dbspj::NullTreeNodePtr(0, RNIL);
const Dbspj::RowRef Dbspj::NullRowRef = {RNIL, GLOBAL_PAGE_SIZE_WORDS};

/**
 * The guarded pointers add an extra level of safety where incoming
 * signals refers internal objects via an 'i-pointer'. The getPtr()
 * method itself offer little protection against 'out of bounds' i-pointers.
 * Thus we maintain the guarded pointers in an internal hash list as well.
 * Using the hash list for looking up untrusty 'i-pointer' guarantees that
 * only valid i-pointers will find their real objects.
 */
void Dbspj::insertGuardedPtr(Ptr<Request> requestPtr,
                             Ptr<TreeNode> treeNodePtr) {
  treeNodePtr.p->key = treeNodePtr.i;
  m_treenode_hash.add(treeNodePtr);
}

void Dbspj::removeGuardedPtr(Ptr<TreeNode> treeNodePtr) {
  m_treenode_hash.remove(treeNodePtr);
}

inline bool Dbspj::getGuardedPtr(Ptr<TreeNode> &treeNodePtr, Uint32 ptrI) {
  /**
   * We could have looked up the pointer directly with getPtr(). However that
   * is regarded unsafe for a 'guarded pointer', as there is no checks
   * in getPtr() for the page_no / pos being within legal bounds.
   * So we use our internal (trusted) hash structures instead and search
   * for an object with the specified 'i-pointer'.
   */
  const bool found = m_treenode_hash.find(treeNodePtr, ptrI);
#if !defined(NDEBUG)
  if (found) {
    Ptr<TreeNode> check;
    ndbrequire(m_treenode_pool.getPtr(check, ptrI));
    ndbassert(check.p == treeNodePtr.p);
    ndbassert(check.i == treeNodePtr.i);
  }
#endif
  return found;
}

void Dbspj::insertGuardedPtr(Ptr<Request> requestPtr,
                             Ptr<ScanFragHandle> scanFragPtr) {
  scanFragPtr.p->key = scanFragPtr.i;
  m_scanfraghandle_hash.add(scanFragPtr);
}

void Dbspj::removeGuardedPtr(Ptr<ScanFragHandle> scanFragPtr) {
  m_scanfraghandle_hash.remove(scanFragPtr);
}

inline bool Dbspj::getGuardedPtr(Ptr<ScanFragHandle> &scanFragPtr,
                                 Uint32 ptrI) {
  const bool found = m_scanfraghandle_hash.find(scanFragPtr, ptrI);
#if !defined(NDEBUG)
  if (found) {
    Ptr<ScanFragHandle> check;
    ndbrequire(m_scanfraghandle_pool.getPtr(check, ptrI));
    ndbassert(check.p == scanFragPtr.p);
    ndbassert(check.i == scanFragPtr.i);
  }
#endif
  return found;
}

void Dbspj::execSIGNAL_DROPPED_REP(Signal *signal) {
  /* An incoming signal was dropped, handle it.
   * Dropped signal really means that we ran out of
   * long signal buffering to store its sections.
   */
  jamEntry();

  if (!assembleDroppedFragments(signal)) {
    jam();
    return;
  }

  const SignalDroppedRep *rep = (SignalDroppedRep *)&signal->theData[0];
  const Uint32 originalGSN = rep->originalGsn;

  DEBUG("SignalDroppedRep received for GSN " << originalGSN);

  switch (originalGSN) {
    case GSN_LQHKEYREQ:  // TC -> SPJ
    {
      jam();
      const LqhKeyReq *const truncatedLqhKeyReq =
          reinterpret_cast<const LqhKeyReq *>(&rep->originalData[0]);

      handle_early_lqhkey_ref(signal, truncatedLqhKeyReq,
                              DbspjErr::OutOfSectionMemory);
      break;
    }
    case GSN_SCAN_FRAGREQ:  // TC -> SPJ
    {
      jam();
      /* Get information necessary to send SCAN_FRAGREF back to TC */
      // TODO : Handle dropped signal fragments

      const ScanFragReq *const truncatedScanFragReq =
          reinterpret_cast<const ScanFragReq *>(&rep->originalData[0]);

      handle_early_scanfrag_ref(signal, truncatedScanFragReq,
                                DbspjErr::OutOfSectionMemory);
      break;
    }
    case GSN_TRANSID_AI:  // TUP -> SPJ
    {
      jam();
      const TransIdAI *const truncatedTransIdAI =
          reinterpret_cast<const TransIdAI *>(&rep->originalData[0]);
      const Uint32 ptrI = truncatedTransIdAI->connectPtr;

      Ptr<TreeNode> treeNodePtr;
      ndbrequire(getGuardedPtr(treeNodePtr, ptrI));
      Ptr<Request> requestPtr;
      ndbrequire(
          m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));

      /**
       * Register signal as arrived -> 'done' if this completed this treeNode
       */
      ndbassert(treeNodePtr.p->m_info && treeNodePtr.p->m_info->m_countSignal);
      (this->*(treeNodePtr.p->m_info->m_countSignal))(signal, requestPtr,
                                                      treeNodePtr, 1);

      abort(signal, requestPtr, DbspjErr::OutOfSectionMemory);
      break;
    }
    default:
      jam();
      /* Don't expect dropped signals for other GSNs */
      SimulatedBlock::execSIGNAL_DROPPED_REP(signal);
  }

#ifdef ERROR_INSERT
  if (ErrorSignalReceive == DBSPJ) {
    jam();
    ErrorSignalReceive = 0;
  }
#endif

  return;
}

inline Uint32 Dbspj::TableRecord::checkTableError(Uint32 schemaVersion) const {
  DEBUG_DICT("Dbspj::TableRecord::checkTableError"
             << ", m_flags: " << m_flags
             << ", m_currentSchemaVersion: " << m_currentSchemaVersion
             << ", check schemaVersion: " << schemaVersion);

  if (!get_enabled()) return DbspjErr::NoSuchTable;
  if (get_dropping()) return DbspjErr::DropTableInProgress;
  if (table_version_major(schemaVersion) !=
      table_version_major(m_currentSchemaVersion))
    return DbspjErr::WrongSchemaVersion;

  return 0;
}

// create table prepare
void Dbspj::execTC_SCHVERREQ(Signal *signal) {
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  const TcSchVerReq *req = CAST_CONSTPTR(TcSchVerReq, signal->getDataPtr());
  const Uint32 tableId = req->tableId;
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;

  DEBUG_DICT("Dbspj::execTC_SCHVERREQ"
             << ", tableId: " << tableId << ", version: " << req->tableVersion);

  TableRecordPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, c_tabrecFilesize, m_tableRecord);

  ndbrequire(tablePtr.p->get_prepared() == false);
  ndbrequire(tablePtr.p->get_enabled() == false);
  new (tablePtr.p) TableRecord(req->tableVersion);

  if (req->readBackup) {
    jam();
    tablePtr.p->m_flags |= TableRecord::TR_READ_BACKUP;
  }

  if (req->fullyReplicated) {
    jam();
    tablePtr.p->m_flags |= TableRecord::TR_FULLY_REPLICATED;
  }

  /**
   * NOTE: Even if there are more information, like
   * 'tableType', 'noOfPrimaryKeys'etc available from
   * TcSchVerReq, we do *not* store that in TableRecord.
   * Instead this information is retrieved on demand from
   * g_key_descriptor_pool where it is readily available.
   * The 'contract' for consistency of this information is
   * such that:
   * 1) g_key_descriptor[ENTRY] will be populated *before*
   *    any blocks receiving CREATE_TAB_REQ (or equivalent).
   * 2) g_key_descriptor[ENTRY] will be invalidated *after*
   *    all blocks sent DROP_TAB_CONF (commit)
   * Thus, this info is consistent whenever required by SPJ.
   */
  TcSchVerConf *conf = (TcSchVerConf *)signal->getDataPtr();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_TC_SCHVERCONF, signal, TcSchVerConf::SignalLength,
             JBB);
}  // Dbspj::execTC_SCHVERREQ()

// create table commit
void Dbspj::execTAB_COMMITREQ(Signal *signal) {
  jamEntry();
  const Uint32 senderData = signal->theData[0];
  const Uint32 senderRef = signal->theData[1];
  const Uint32 tableId = signal->theData[2];

  DEBUG_DICT("Dbspj::execTAB_COMMITREQ"
             << ", tableId: " << tableId);

  TableRecordPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, c_tabrecFilesize, m_tableRecord);

  ndbrequire(tablePtr.p->get_prepared() == true);
  ndbrequire(tablePtr.p->get_enabled() == false);
  tablePtr.p->set_enabled(true);
  tablePtr.p->set_prepared(false);
  tablePtr.p->set_dropping(false);

  signal->theData[0] = senderData;
  signal->theData[1] = reference();
  signal->theData[2] = tableId;
  sendSignal(senderRef, GSN_TAB_COMMITCONF, signal, 3, JBB);
}  // Dbspj::execTAB_COMMITREQ

void Dbspj::execPREP_DROP_TAB_REQ(Signal *signal) {
  jamEntry();

  PrepDropTabReq *req = (PrepDropTabReq *)signal->getDataPtr();
  const Uint32 tableId = req->tableId;
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;

  DEBUG_DICT("Dbspj::execPREP_DROP_TAB_REQ"
             << ", tableId: " << tableId);

  TableRecordPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, c_tabrecFilesize, m_tableRecord);

  if (!tablePtr.p->get_enabled()) {
    jam();
    PrepDropTabRef *ref = (PrepDropTabRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tableId;
    ref->errorCode = PrepDropTabRef::NoSuchTable;
    sendSignal(senderRef, GSN_PREP_DROP_TAB_REF, signal,
               PrepDropTabRef::SignalLength, JBB);
    return;
  }

  if (tablePtr.p->get_dropping()) {
    jam();
    PrepDropTabRef *ref = (PrepDropTabRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tableId;
    ref->errorCode = PrepDropTabRef::DropInProgress;
    sendSignal(senderRef, GSN_PREP_DROP_TAB_REF, signal,
               PrepDropTabRef::SignalLength, JBB);
    return;
  }

  tablePtr.p->set_dropping(true);
  tablePtr.p->set_prepared(false);

  PrepDropTabConf *conf = (PrepDropTabConf *)signal->getDataPtrSend();
  conf->tableId = tableId;
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_PREP_DROP_TAB_CONF, signal,
             PrepDropTabConf::SignalLength, JBB);
}  // Dbspj::execPREP_DROP_TAB_REQ

void Dbspj::execDROP_TAB_REQ(Signal *signal) {
  jamEntry();

  const DropTabReq *req = (DropTabReq *)signal->getDataPtr();
  const Uint32 tableId = req->tableId;
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  DropTabReq::RequestType rt = (DropTabReq::RequestType)req->requestType;

  DEBUG_DICT("Dbspj::execDROP_TAB_REQ"
             << ", tableId: " << tableId);

  TableRecordPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, c_tabrecFilesize, m_tableRecord);

  if (rt == DropTabReq::OnlineDropTab) {
    if (!tablePtr.p->get_enabled()) {
      jam();
      DropTabRef *ref = (DropTabRef *)signal->getDataPtrSend();
      ref->senderRef = reference();
      ref->senderData = senderData;
      ref->tableId = tableId;
      ref->errorCode = DropTabRef::NoSuchTable;
      sendSignal(senderRef, GSN_DROP_TAB_REF, signal, DropTabRef::SignalLength,
                 JBB);
      return;
    }
    if (!tablePtr.p->get_dropping()) {
      jam();
      DropTabRef *ref = (DropTabRef *)signal->getDataPtrSend();
      ref->senderRef = reference();
      ref->senderData = senderData;
      ref->tableId = tableId;
      ref->errorCode = DropTabRef::DropWoPrep;
      sendSignal(senderRef, GSN_DROP_TAB_REF, signal, DropTabRef::SignalLength,
                 JBB);
      return;
    }
  }

  tablePtr.p->set_enabled(false);
  tablePtr.p->set_prepared(false);
  tablePtr.p->set_dropping(false);

  DropTabConf *conf = (DropTabConf *)signal->getDataPtrSend();
  conf->tableId = tableId;
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_DROP_TAB_CONF, signal,
             PrepDropTabConf::SignalLength, JBB);
}  // Dbspj::execDROP_TAB_REQ

void Dbspj::execALTER_TAB_REQ(Signal *signal) {
  jamEntry();

  const AlterTabReq *req = (const AlterTabReq *)signal->getDataPtr();
  const Uint32 tableId = req->tableId;
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 tableVersion = req->tableVersion;
  const Uint32 newTableVersion = req->newTableVersion;
  AlterTabReq::RequestType requestType =
      (AlterTabReq::RequestType)req->requestType;
  D("ALTER_TAB_REQ(SPJ)");

  DEBUG_DICT("Dbspj::execALTER_TAB_REQ"
             << ", tableId: " << tableId << ", version: " << tableVersion
             << " --> " << newTableVersion);

  TableRecordPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, c_tabrecFilesize, m_tableRecord);

  switch (requestType) {
    case AlterTabReq::AlterTablePrepare:
      jam();
      break;
    case AlterTabReq::AlterTableRevert:
      jam();
      tablePtr.p->m_currentSchemaVersion = tableVersion;
      break;
    case AlterTabReq::AlterTableCommit:
      jam();
      tablePtr.p->m_currentSchemaVersion = newTableVersion;
      if (AlterTableReq::getReadBackupFlag(req->changeMask)) {
        /**
         * We simply swap the flag, the preparatory work for this
         * change is done in DBTC.
         */
        if ((tablePtr.p->m_flags & TableRecord::TR_READ_BACKUP) != 0) {
          jam();
          /* Reset Read Backup flag */
          tablePtr.p->m_flags &= (~(TableRecord::TR_READ_BACKUP));
        } else {
          jam();
          /* Set Read Backup flag */
          tablePtr.p->m_flags |= TableRecord::TR_READ_BACKUP;
        }
      }
      break;
    default:
      ndbabort();
  }

  AlterTabConf *conf = (AlterTabConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  conf->connectPtr = RNIL;
  sendSignal(senderRef, GSN_ALTER_TAB_CONF, signal, AlterTabConf::SignalLength,
             JBB);
}  // Dbspj::execALTER_TAB_REQ

/** A noop for now.*/
void Dbspj::execREAD_CONFIG_REQ(Signal *signal) {
  jamEntry();
  const ReadConfigReq req =
      *reinterpret_cast<const ReadConfigReq *>(signal->getDataPtr());

  Pool_context pc;
  pc.m_block = this;

  DEBUG("execREAD_CONFIG_REQ");
  DEBUG("sizeof(Request): " << sizeof(Request)
                            << " sizeof(TreeNode): " << sizeof(TreeNode));

  m_arenaAllocator.init(1024, RT_SPJ_ARENA_BLOCK, pc);
  m_request_pool.arena_pool_init(&m_arenaAllocator, RT_SPJ_REQUEST, pc);
  m_treenode_pool.arena_pool_init(&m_arenaAllocator, RT_SPJ_TREENODE, pc);
  m_scanfraghandle_pool.arena_pool_init(&m_arenaAllocator, RT_SPJ_SCANFRAG, pc);
  m_lookup_request_hash.setSize(16);
  m_scan_request_hash.setSize(16);
  m_treenode_hash.setSize(256);
  m_scanfraghandle_hash.setSize(1024);
  void *ptr = m_ctx.m_mm.get_memroot();
  m_page_pool.set((RowPage *)ptr, (Uint32)~0);

  Record_info ri;
  Dependency_map::createRecordInfo(ri, RT_SPJ_DATABUFFER);
  m_dependency_map_pool.init(&m_arenaAllocator, ri, pc);

  {
    const ndb_mgm_configuration_iterator *p =
        m_ctx.m_config.getOwnConfigIterator();
    ndbrequire(p != 0);

    ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_SPJ_TABLE, &c_tabrecFilesize));
  }
  m_tableRecord = (TableRecord *)allocRecord("TableRecord", sizeof(TableRecord),
                                             c_tabrecFilesize);

  TableRecordPtr tablePtr;
  for (tablePtr.i = 0; tablePtr.i < c_tabrecFilesize; tablePtr.i++) {
    ptrAss(tablePtr, m_tableRecord);
    new (tablePtr.p) TableRecord;
  }  // for

  ReadConfigConf *const conf =
      reinterpret_cast<ReadConfigConf *>(signal->getDataPtrSend());
  conf->senderRef = reference();
  conf->senderData = req.senderData;

  sendSignal(req.senderRef, GSN_READ_CONFIG_CONF, signal,
             ReadConfigConf::SignalLength, JBB);
}  // Dbspj::execREAD_CONF_REQ()

static Uint32 f_STTOR_REF = 0;

void Dbspj::execSTTOR(Signal *signal) {
  // #define UNIT_TEST_DATABUFFER2

  jamEntry();
  /* START CASE */
  const Uint16 tphase = signal->theData[1];
  f_STTOR_REF = signal->getSendersBlockRef();

  if (tphase == 1) {
    jam();
    signal->theData[0] = 0;  // 0 -> Start the releaseGlobal() 'thread'
    signal->theData[1] = 0;  // 0 -> ... and sample usage statistics
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 2);
    c_tc = (Dbtc *)globalData.getBlock(DBTC, instance());
  }

  if (tphase == 4) {
    jam();

    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    return;
  }

  sendSTTORRY(signal);

#ifdef UNIT_TEST_DATABUFFER2
  if (tphase == 120) {
    g_eventLogger->info("basic test of ArenaPool / DataBuffer");

    for (Uint32 i = 0; i < 100; i++) {
      ArenaHead ah;
      if (!m_arenaAllocator.seize(ah)) {
        g_eventLogger->info("Failed to allocate arena");
        break;
      }

      g_eventLogger->info("*** LOOP %u", i);
      Uint32 sum = 0;
      Dependency_map::Head head;
      LocalArenaPool<DataBufferSegment<14>> pool(ah, m_dependency_map_pool);
      for (Uint32 j = 0; j < 100; j++) {
        Uint32 sz = rand() % 1000;
        if (0) g_eventLogger->info("adding %u", sz);
        Local_dependency_map list(pool, head);
        for (Uint32 i = 0; i < sz; i++) signal->theData[i] = sum + i;
        list.append(signal->theData, sz);
        sum += sz;
      }

      {
        ndbrequire(head.getSize() == sum);
        Local_dependency_map list(pool, head);
        Dependency_map::ConstDataBufferIterator it;
        Uint32 cnt = 0;
        for (list.first(it); !it.isNull(); list.next(it)) {
          ndbrequire(*it.data == cnt);
          cnt++;
        }

        ndbrequire(cnt == sum);
      }

      Resource_limit rl;
      if (m_ctx.m_mm.get_resource_limit(7, rl)) {
        g_eventLogger->info("Resource %d min: %d max: %d curr: %d", 7, rl.m_min,
                            rl.m_max, rl.m_curr);
      }

      {
        g_eventLogger->info("release map");
        Local_dependency_map list(pool, head);
        list.release();
      }

      g_eventLogger->info("release all");
      m_arenaAllocator.release(ah);
      g_eventLogger->info("*** LOOP %u sum: %u", i, sum);
    }
  }
#endif
}  // Dbspj::execSTTOR()

void Dbspj::sendSTTORRY(Signal *signal) {
  signal->theData[0] = 0;
  signal->theData[1] = 0; /* BLOCK CATEGORY */
  signal->theData[2] = 0; /* SIGNAL VERSION NUMBER */
  signal->theData[3] = 4;
#ifdef UNIT_TEST_DATABUFFER2
  signal->theData[4] = 120; /* Start phase end*/
#else
  signal->theData[4] = 255;
#endif
  signal->theData[5] = 255;
  sendSignal(f_STTOR_REF, GSN_STTORRY, signal, 6, JBB);
}

void Dbspj::execREAD_NODESCONF(Signal *signal) {
  jamEntry();

  ReadNodesConf *const conf = (ReadNodesConf *)signal->getDataPtr();
  {
    ndbrequire(signal->getNoOfSections() == 1);
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz == 5 * NdbNodeBitmask::Size);
    copy((Uint32 *)&conf->definedNodes.rep.data, ptr);
    releaseSections(handle);
  }

  if (getNodeState().getNodeRestartInProgress()) {
    jam();
    c_alive_nodes = conf->startedNodes;
    c_alive_nodes.set(getOwnNodeId());
  } else {
    jam();
    c_alive_nodes = conf->startingNodes;
    NdbNodeBitmask tmp = conf->startedNodes;
    c_alive_nodes.bitOR(tmp);
  }

  for (Uint32 i = 0; i < MAX_NDB_NODES; i++) {
    m_location_domain_id[i] = 0;
  }

  ndb_mgm_configuration_iterator *p_iter =
      ndb_mgm_create_configuration_iterator(m_ctx.m_config.getClusterConfig(),
                                            CFG_SECTION_NODE);
  for (ndb_mgm_first(p_iter); ndb_mgm_valid(p_iter); ndb_mgm_next(p_iter)) {
    jam();
    Uint32 location_domain_id = 0;
    Uint32 nodeId = 0;
    Uint32 nodeType = 0;
    ndbrequire(!ndb_mgm_get_int_parameter(p_iter, CFG_NODE_ID, &nodeId) &&
               nodeId != 0);
    jamLine(Uint16(nodeId));
    ndbrequire(
        !ndb_mgm_get_int_parameter(p_iter, CFG_TYPE_OF_SECTION, &nodeType));
    ndbrequire(nodeId != 0);
    if (nodeType != NODE_TYPE_DB) {
      jam();
      continue;
    }
    ndbrequire(nodeId < MAX_NDB_NODES);
    ndb_mgm_get_int_parameter(p_iter, CFG_LOCATION_DOMAIN_ID,
                              &location_domain_id);
    m_location_domain_id[nodeId] = location_domain_id;
  }
  ndb_mgm_destroy_iterator(p_iter);
  sendSTTORRY(signal);
}

void Dbspj::execINCL_NODEREQ(Signal *signal) {
  jamEntry();
  const Uint32 senderRef = signal->theData[0];
  const Uint32 nodeId = signal->theData[1];

  ndbrequire(!c_alive_nodes.get(nodeId));
  c_alive_nodes.set(nodeId);

  signal->theData[0] = nodeId;
  signal->theData[1] = reference();
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 2, JBB);
}

void Dbspj::execNODE_FAILREP(Signal *signal) {
  jamEntry();

  NodeFailRep *rep = (NodeFailRep *)signal->getDataPtr();
  if (signal->getLength() == NodeFailRep::SignalLength) {
    ndbrequire(signal->getNoOfSections() == 1);
    ndbrequire(getNodeInfo(refToNode(signal->getSendersBlockRef())).m_version);
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    memset(rep->theNodes, 0, sizeof(rep->theNodes));
    copy(rep->theNodes, ptr);
    releaseSections(handle);
  } else {
    memset(rep->theNodes + NdbNodeBitmask48::Size, 0, _NDB_NBM_DIFF_BYTES);
  }
  NdbNodeBitmask failed;
  failed.assign(NdbNodeBitmask::Size, rep->theNodes);

  c_alive_nodes.bitANDC(failed);

  /* Clean up possibly fragmented signals being received or sent */
  for (Uint32 node = 1; node < MAX_NDB_NODES; node++) {
    if (failed.get(node)) {
      jam();
      simBlockNodeFailure(signal, node);
    }  // if
  }    // for

  signal->theData[0] = 1;
  signal->theData[1] = 0;
  failed.copyto(NdbNodeBitmask::Size, signal->theData + 2);
  LinearSectionPtr lsptr[3];
  lsptr[0].p = signal->theData + 2;
  lsptr[0].sz = failed.getPackedLengthInWords();
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB, lsptr, 1);
}

void Dbspj::execAPI_FAILREQ(Signal *signal) {
  jamEntry();
  Uint32 failedApiNode = signal->theData[0];
  Uint32 ref = signal->theData[1];

  /**
   * We only need to care about lookups
   *   as SCAN's are aborted by DBTC
   *
   * As SPJ does not receive / send fragmented signals
   *   directly to API nodes, simBlockNodeFailure()
   *   should not really be required - assert this.
   */
  Uint32 elementsCleaned = simBlockNodeFailure(signal, failedApiNode);
  ndbassert(elementsCleaned == 0);  // As SPJ has no fragmented API signals
  (void)elementsCleaned;            // Avoid compiler error

  signal->theData[0] = failedApiNode;
  signal->theData[1] = reference();
  sendSignal(ref, GSN_API_FAILCONF, signal, 2, JBB);
}

void Dbspj::execCONTINUEB(Signal *signal) {
  jamEntry();
  switch (signal->theData[0]) {
    case 0:
      releaseGlobal(signal);
      return;
    case 1:
      nodeFail_checkRequests(signal);
      return;
    case 2:
      nodeFail_checkRequests(signal);
      return;
    case 3: {
      Ptr<TreeNode> treeNodePtr;
      Ptr<Request> requestPtr;
      ndbrequire(getGuardedPtr(treeNodePtr, signal->theData[1]));
      ndbrequire(
          m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));
      scanFrag_sendDihGetNodesReq(signal, requestPtr, treeNodePtr);
      checkPrepareComplete(signal, requestPtr);
      return;
    }
  }

  ndbabort();
}

void Dbspj::nodeFail_checkRequests(Signal *signal) {
  jam();
  const Uint32 type = signal->theData[0];
  const Uint32 bucket = signal->theData[1];

  NdbNodeBitmask failed;
  ndbrequire(signal->getNoOfSections() == 1);

  SegmentedSectionPtr ptr;
  SectionHandle handle(this, signal);
  ndbrequire(handle.getSection(ptr, 0));
  ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
  copy(failed.rep.data, ptr);
  releaseSections(handle);

  Request_iterator iter;
  Request_hash *hash = NULL;
  switch (type) {
    case 1:
      hash = &m_lookup_request_hash;
      break;
    case 2:
      hash = &m_scan_request_hash;
      break;
    default:
      hash = NULL;  // Silence compiler warning
      ndbabort();   // Impossible, avoid warning
  }
  hash->next(bucket, iter);

  const Uint32 RT_BREAK = 64;
  for (Uint32 i = 0;
       (i < RT_BREAK || iter.bucket == bucket) && !iter.curr.isNull(); i++) {
    jam();

    Ptr<Request> requestPtr = iter.curr;
    hash->next(iter);
    i += nodeFail(signal, requestPtr, failed);
  }

  if (!iter.curr.isNull()) {
    jam();
    signal->theData[0] = type;
    signal->theData[1] = bucket;
    failed.copyto(NdbNodeBitmask::Size, signal->theData + 2);
    LinearSectionPtr lsptr[3];
    lsptr[0].p = signal->theData + 2;
    lsptr[0].sz = failed.getPackedLengthInWords();
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB, lsptr, 1);
  } else if (type == 1) {
    jam();
    signal->theData[0] = 2;
    signal->theData[1] = 0;
    failed.copyto(NdbNodeBitmask::Size, signal->theData + 2);
    LinearSectionPtr lsptr[3];
    lsptr[0].p = signal->theData + 2;
    lsptr[0].sz = failed.getPackedLengthInWords();
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB, lsptr, 1);
  } else if (type == 2) {
    jam();
  }
}

/**
 * MODULE LQHKEYREQ
 */
void Dbspj::execLQHKEYREQ(Signal *signal) {
  jamEntry();
  if (unlikely(!assembleFragments(signal))) {
    jam();
    return;
  }

  c_Counters.incr_counter(CI_READS_RECEIVED, 1);

  if (ERROR_INSERTED(17014)) {
    ndbrequire(refToNode(signal->getSendersBlockRef()) == getOwnNodeId());
  }

  const LqhKeyReq *req =
      reinterpret_cast<const LqhKeyReq *>(signal->getDataPtr());

  /**
   * #0 - KEYINFO contains key for first operation (used for hash in TC)
   * #1 - ATTRINFO contains tree + parameters
   *      (unless StoredProcId is set, when only parameters are sent,
   *       but this is not yet implemented)
   */
  SegmentedSectionPtr attrPtr;
  SectionHandle handle(this, signal);
  ndbrequire(handle.getSection(attrPtr, LqhKeyReq::AttrInfoSectionNum));
  const Uint32 keyPtrI = handle.m_ptr[LqhKeyReq::KeyInfoSectionNum].i;

  Uint32 err;
  Ptr<Request> requestPtr(0, RNIL);
  bool in_hash = false;
  do {
    ArenaHead ah;
    err = DbspjErr::OutOfQueryMemory;
    if (unlikely(!m_arenaAllocator.seize(ah))) break;

    if (ERROR_INSERTED_CLEAR(17001)) {
      jam();
      g_eventLogger->info(
          "Injecting OutOfQueryMem error 17001 at line %d file %s", __LINE__,
          __FILE__);
      break;
    }
    if (unlikely(!m_request_pool.seize(ah, requestPtr))) {
      jam();
      break;
    }
    new (requestPtr.p) Request(ah);
    do_init(requestPtr.p, req, signal->getSendersBlockRef());

    Uint32 len_cnt;

    {
      SectionReader r0(attrPtr, getSectionSegmentPool());

      err = DbspjErr::ZeroLengthQueryTree;
      if (unlikely(!r0.getWord(&len_cnt))) break;
    }

    Uint32 len = QueryTree::getLength(len_cnt);
    Uint32 cnt = QueryTree::getNodeCnt(len_cnt);

    {
      SectionReader treeReader(attrPtr, getSectionSegmentPool());
      SectionReader paramReader(attrPtr, getSectionSegmentPool());
      paramReader.step(len);  // skip over tree to parameters

      Build_context ctx;
      ctx.m_resultRef = req->variableData[0];
      ctx.m_savepointId = req->savePointId;
      ctx.m_scanPrio = 1;
      ctx.m_start_signal = signal;
      ctx.m_senderRef = signal->getSendersBlockRef();

      err = build(ctx, requestPtr, treeReader, paramReader);
      if (unlikely(err != 0)) break;

      /**
       * Root TreeNode in Request takes ownership of keyPtr
       * section when build has completed.
       * We are done with attrPtr which are now released.
       */
      Ptr<TreeNode> rootNodePtr = ctx.m_node_list[0];
      rootNodePtr.p->m_send.m_keyInfoPtrI = keyPtrI;
      release(attrPtr);
      handle.clear();
    }

    /**
     * Store request in list(s)/hash(es)
     */
    store_lookup(requestPtr);
    in_hash = true;

    /**
     * A query being shipped as a LQHKEYREQ may return at most a row
     * per operation i.e be a (multi-)lookup
     */
    if (ERROR_INSERTED_CLEAR(17013) ||
        unlikely(!requestPtr.p->isLookup() ||
                 requestPtr.p->m_node_cnt != cnt)) {
      jam();
      err = DbspjErr::InvalidRequest;
      break;
    }

    prepare(signal, requestPtr);
    checkPrepareComplete(signal, requestPtr);
    return;
  } while (0);

  /**
   * Error handling below,
   *  'err' should contain error code.
   */
  ndbassert(err != 0);
  if (!requestPtr.isNull()) {
    jam();
    cleanup(requestPtr, in_hash);
  }
  releaseSections(handle);  // a NOOP, if we reached 'handle.clear()' above
  handle_early_lqhkey_ref(signal, req, err);
}

void Dbspj::do_init(Request *requestP, const LqhKeyReq *req, Uint32 senderRef) {
  requestP->m_bits = 0;
  requestP->m_errCode = 0;
  requestP->m_state = Request::RS_BUILDING;
  requestP->m_node_cnt = 0;
  requestP->m_cnt_active = 0;
  requestP->m_rows = 0;
  requestP->m_active_tree_nodes.clear();
  requestP->m_completed_tree_nodes.set();
  requestP->m_suspended_tree_nodes.clear();
  requestP->m_outstanding = 0;
  requestP->m_transId[0] = req->transId1;
  requestP->m_transId[1] = req->transId2;
  requestP->m_rootFragId = LqhKeyReq::getFragmentId(req->fragmentData);
  requestP->m_rootFragCnt = 1;
  std::memset(requestP->m_lookup_node_data, 0,
              sizeof(requestP->m_lookup_node_data));
#ifdef SPJ_TRACE_TIME
  requestP->m_cnt_batches = 0;
  requestP->m_sum_rows = 0;
  requestP->m_sum_running = 0;
  requestP->m_sum_waiting = 0;
  requestP->m_save_time = NdbTick_getCurrentTicks();
#endif
  const Uint32 reqInfo = req->requestInfo;
  Uint32 tmp = req->clientConnectPtr;
  if (LqhKeyReq::getDirtyFlag(reqInfo) &&
      LqhKeyReq::getOperation(reqInfo) == ZREAD) {
    jam();

    ndbrequire(LqhKeyReq::getApplicationAddressFlag(reqInfo));
    // const Uint32 apiRef   = lqhKeyReq->variableData[0];
    // const Uint32 apiOpRec = lqhKeyReq->variableData[1];
    tmp = req->variableData[1];
    requestP->m_senderData = tmp;
    requestP->m_senderRef = senderRef;
  } else {
    if (LqhKeyReq::getSameClientAndTcFlag(reqInfo) == 1) {
      if (LqhKeyReq::getApplicationAddressFlag(reqInfo))
        tmp = req->variableData[2];
      else
        tmp = req->variableData[0];
    }
    requestP->m_senderData = tmp;
    requestP->m_senderRef = senderRef;
  }
  requestP->m_rootResultData = tmp;
}

void Dbspj::store_lookup(Ptr<Request> requestPtr) {
  ndbassert(requestPtr.p->isLookup());
  Ptr<Request> tmp;
  bool found = m_lookup_request_hash.find(tmp, *requestPtr.p);
  ndbrequire(found == false);
  m_lookup_request_hash.add(requestPtr);
}

void Dbspj::handle_early_lqhkey_ref(Signal *signal, const LqhKeyReq *lqhKeyReq,
                                    Uint32 err) {
  /**
   * Error path...
   */
  ndbrequire(err);
  const Uint32 reqInfo = lqhKeyReq->requestInfo;
  const Uint32 transid[2] = {lqhKeyReq->transId1, lqhKeyReq->transId2};

  if (LqhKeyReq::getDirtyFlag(reqInfo) &&
      LqhKeyReq::getOperation(reqInfo) == ZREAD) {
    jam();
    /* Dirty read sends TCKEYREF direct to client, and nothing to TC */
    ndbrequire(LqhKeyReq::getApplicationAddressFlag(reqInfo));
    const Uint32 apiRef = lqhKeyReq->variableData[0];
    const Uint32 apiOpRec = lqhKeyReq->variableData[1];

    TcKeyRef *const tcKeyRef =
        reinterpret_cast<TcKeyRef *>(signal->getDataPtrSend());

    tcKeyRef->connectPtr = apiOpRec;
    tcKeyRef->transId[0] = transid[0];
    tcKeyRef->transId[1] = transid[1];
    tcKeyRef->errorCode = err;
    sendTCKEYREF(signal, apiRef, signal->getSendersBlockRef());
  } else {
    jam();
    const Uint32 returnref = signal->getSendersBlockRef();
    const Uint32 clientPtr = lqhKeyReq->clientConnectPtr;

    Uint32 TcOprec = clientPtr;
    if (LqhKeyReq::getSameClientAndTcFlag(reqInfo) == 1) {
      if (LqhKeyReq::getApplicationAddressFlag(reqInfo))
        TcOprec = lqhKeyReq->variableData[2];
      else
        TcOprec = lqhKeyReq->variableData[0];
    }

    LqhKeyRef *const ref =
        reinterpret_cast<LqhKeyRef *>(signal->getDataPtrSend());
    ref->userRef = clientPtr;
    ref->connectPtr = TcOprec;
    ref->errorCode = err;
    ref->transId1 = transid[0];
    ref->transId2 = transid[1];
    ref->flags = 0;
    sendSignal(returnref, GSN_LQHKEYREF, signal, LqhKeyRef::SignalLength, JBB);
  }
}

void Dbspj::sendTCKEYREF(Signal *signal, Uint32 ref, Uint32 routeRef) {
  const Uint32 nodeId = refToNode(ref);
  const bool connectedToNode = getNodeInfo(nodeId).m_connected;

  if (likely(connectedToNode)) {
    jam();
    sendSignal(ref, GSN_TCKEYREF, signal, TcKeyRef::SignalLength, JBB);
  } else {
    jam();
    memmove(signal->theData + 25, signal->theData, 4 * TcKeyRef::SignalLength);
    RouteOrd *ord = (RouteOrd *)signal->getDataPtrSend();
    ord->dstRef = ref;
    ord->srcRef = reference();
    ord->gsn = GSN_TCKEYREF;
    ord->cnt = 0;
    LinearSectionPtr ptr[3];
    ptr[0].p = signal->theData + 25;
    ptr[0].sz = TcKeyRef::SignalLength;
    sendSignal(routeRef, GSN_ROUTE_ORD, signal, RouteOrd::SignalLength, JBB,
               ptr, 1);
  }
}

void Dbspj::sendTCKEYCONF(Signal *signal, Uint32 len, Uint32 ref,
                          Uint32 routeRef) {
  const Uint32 nodeId = refToNode(ref);
  const bool connectedToNode = getNodeInfo(nodeId).m_connected;

  if (likely(connectedToNode)) {
    jam();
    sendSignal(ref, GSN_TCKEYCONF, signal, len, JBB);
  } else {
    jam();
    memmove(signal->theData + 25, signal->theData, 4 * len);
    RouteOrd *ord = (RouteOrd *)signal->getDataPtrSend();
    ord->dstRef = ref;
    ord->srcRef = reference();
    ord->gsn = GSN_TCKEYCONF;
    ord->cnt = 0;
    LinearSectionPtr ptr[3];
    ptr[0].p = signal->theData + 25;
    ptr[0].sz = len;
    sendSignal(routeRef, GSN_ROUTE_ORD, signal, RouteOrd::SignalLength, JBB,
               ptr, 1);
  }
}

/**
 * END - MODULE LQHKEYREQ
 */

/**
 * MODULE SCAN_FRAGREQ
 */
void Dbspj::execSCAN_FRAGREQ(Signal *signal) {
  jamEntry();

  /* Reassemble if the request was fragmented */
  if (!assembleFragments(signal)) {
    jam();
    return;
  }

  if (ERROR_INSERTED(17014)) {
    ndbrequire(refToNode(signal->getSendersBlockRef()) == getOwnNodeId());
  }

  const ScanFragReq *req = (ScanFragReq *)&signal->theData[0];

#ifdef DEBUG_SCAN_FRAGREQ
  g_eventLogger->info("Incoming SCAN_FRAGREQ ");
  printSCAN_FRAGREQ(stdout, signal->getDataPtrSend(),
                    ScanFragReq::SignalLength + 2, DBLQH);
#endif

  /**
   * #0 - ATTRINFO contains tree + parameters
   *      (unless StoredProcId is set, when only parameters are sent,
   *       but this is not yet implemented)
   * #1 - KEYINFO if first op is index scan - contains bounds for first scan
   *              if first op is lookup - contains keyinfo for lookup
   */
  SectionHandle handle(this, signal);
  SegmentedSectionPtr attrPtr;
  ndbrequire(handle.getSection(attrPtr, ScanFragReq::AttrInfoSectionNum));

  Uint32 err;
  Ptr<Request> requestPtr(0, RNIL);
  bool in_hash = false;
  do {
    ArenaHead ah;
    err = DbspjErr::OutOfQueryMemory;
    if (unlikely(!m_arenaAllocator.seize(ah))) break;

    if (ERROR_INSERTED_CLEAR(17002)) {
      g_eventLogger->info(
          "Injecting OutOfQueryMem error 17002 at line %d file %s", __LINE__,
          __FILE__);
      jam();
      break;
    }
    if (unlikely(!m_request_pool.seize(ah, requestPtr))) {
      jam();
      break;
    }
    new (requestPtr.p) Request(ah);
    do_init(requestPtr.p, req, signal->getSendersBlockRef());

    Uint32 len_cnt;
    {
      SectionReader r0(attrPtr, getSectionSegmentPool());
      err = DbspjErr::ZeroLengthQueryTree;
      if (unlikely(!r0.getWord(&len_cnt))) break;
    }

    Uint32 len = QueryTree::getLength(len_cnt);
    Uint32 cnt = QueryTree::getNodeCnt(len_cnt);

    Uint32 sectionCnt = handle.m_cnt;
    Uint32 fragIdsPtrI = RNIL;
    if (ScanFragReq::getMultiFragFlag(req->requestInfo)) {
      jam();
      sectionCnt--;
      fragIdsPtrI = handle.m_ptr[sectionCnt].i;
      SectionReader fragsReader(fragIdsPtrI, getSectionSegmentPool());

      // Unpack into extended signal memory:
      const Uint32 fragCnt = signal->theData[25] = fragsReader.getSize();
      if (unlikely(!fragsReader.getWords(&signal->theData[26], fragCnt))) {
        jam();
        err = DbspjErr::InvalidRequest;
        break;
      }
    }

    {
      SectionReader treeReader(attrPtr, getSectionSegmentPool());
      SectionReader paramReader(attrPtr, getSectionSegmentPool());
      paramReader.step(len);  // skip over tree to parameters

      Build_context ctx;
      ctx.m_resultRef = req->resultRef;
      ctx.m_scanPrio = ScanFragReq::getScanPrio(req->requestInfo);
      ctx.m_savepointId = req->savePointId;
      ctx.m_start_signal = signal;
      ctx.m_senderRef = signal->getSendersBlockRef();

      err = build(ctx, requestPtr, treeReader, paramReader);
      if (unlikely(err != 0)) break;

      /**
       * Root TreeNode in Request takes ownership of keyPtr
       * section when build has completed.
       * We are done with attrPtr and MultiFrag-list which are
       * now released.
       */
      Ptr<TreeNode> rootNodePtr = ctx.m_node_list[0];
      if (sectionCnt > ScanFragReq::KeyInfoSectionNum) {
        jam();
        sectionCnt--;
        const Uint32 keyPtrI = handle.m_ptr[ScanFragReq::KeyInfoSectionNum].i;
        rootNodePtr.p->m_send.m_keyInfoPtrI = keyPtrI;
      }
      release(attrPtr);
      releaseSection(fragIdsPtrI);  // MultiFrag list
      handle.clear();
    }

    /**
     * Store request in list(s)/hash(es)
     */
    store_scan(requestPtr);
    in_hash = true;

    if (ERROR_INSERTED_CLEAR(17013) ||
        unlikely(!requestPtr.p->isScan() || requestPtr.p->m_node_cnt != cnt)) {
      jam();
      err = DbspjErr::InvalidRequest;
      break;
    }

    prepare(signal, requestPtr);
    checkPrepareComplete(signal, requestPtr);
    return;
  } while (0);

  /**
   * Error handling below,
   *  'err' should contain error code.
   */
  ndbassert(err != 0);
  if (!requestPtr.isNull()) {
    jam();
    cleanup(requestPtr, in_hash);
  }
  releaseSections(handle);  // a NOOP, if we reached 'handle.clear()' above
  handle_early_scanfrag_ref(signal, req, err);
}

void Dbspj::do_init(Request *requestP, const ScanFragReq *req,
                    Uint32 senderRef) {
  requestP->m_bits = Request::RT_SCAN;
  requestP->m_errCode = 0;
  requestP->m_state = Request::RS_BUILDING;
  requestP->m_node_cnt = 0;
  requestP->m_cnt_active = 0;
  requestP->m_rows = 0;
  requestP->m_active_tree_nodes.clear();
  requestP->m_completed_tree_nodes.set();
  requestP->m_suspended_tree_nodes.clear();
  requestP->m_outstanding = 0;
  requestP->m_senderRef = senderRef;
  requestP->m_senderData = req->senderData;
  requestP->m_transId[0] = req->transId1;
  requestP->m_transId[1] = req->transId2;
  requestP->m_rootResultData = req->resultData;
  requestP->m_rootFragId = req->fragmentNoKeyLen;
  requestP->m_rootFragCnt = 0;  // Filled in later
  std::memset(requestP->m_lookup_node_data, 0,
              sizeof(requestP->m_lookup_node_data));
#ifdef SPJ_TRACE_TIME
  requestP->m_cnt_batches = 0;
  requestP->m_sum_rows = 0;
  requestP->m_sum_running = 0;
  requestP->m_sum_waiting = 0;
  requestP->m_save_time = NdbTick_getCurrentTicks();
#endif
}

void Dbspj::store_scan(Ptr<Request> requestPtr) {
  ndbassert(requestPtr.p->isScan());
  Ptr<Request> tmp;
  bool found = m_scan_request_hash.find(tmp, *requestPtr.p);
  ndbrequire(found == false);
  m_scan_request_hash.add(requestPtr);
}

void Dbspj::handle_early_scanfrag_ref(Signal *signal, const ScanFragReq *_req,
                                      Uint32 err) {
  ScanFragReq req = *_req;
  Uint32 senderRef = signal->getSendersBlockRef();

  ScanFragRef *ref = (ScanFragRef *)&signal->theData[0];
  ref->senderData = req.senderData;
  ref->transId1 = req.transId1;
  ref->transId2 = req.transId2;
  ref->errorCode = err;
  sendSignal(senderRef, GSN_SCAN_FRAGREF, signal, ScanFragRef::SignalLength,
             JBB);
}

/**
 * END - MODULE SCAN_FRAGREQ
 */

/**
 * MODULE GENERIC
 */
Uint32 Dbspj::build(Build_context &ctx, Ptr<Request> requestPtr,
                    SectionReader &tree, SectionReader &param) {
  Uint32 tmp0, tmp1;
  Uint32 err = DbspjErr::ZeroLengthQueryTree;
  ctx.m_cnt = 0;
  ctx.m_scan_cnt = 0;

  tree.getWord(&tmp0);
  Uint32 loop = QueryTree::getNodeCnt(tmp0);

  DEBUG("::build()");
  err = DbspjErr::InvalidTreeNodeCount;
  if (loop == 0 || loop > NDB_SPJ_MAX_TREE_NODES) {
    jam();
    goto error;
  }

  while (ctx.m_cnt < loop) {
    DEBUG(" - loop " << ctx.m_cnt << " pos: " << tree.getPos().currPos);
    tree.peekWord(&tmp0);
    param.peekWord(&tmp1);
    Uint32 node_op = QueryNode::getOpType(tmp0);
    Uint32 node_len = QueryNode::getLength(tmp0);
    Uint32 param_op = QueryNodeParameters::getOpType(tmp1);
    Uint32 param_len = QueryNodeParameters::getLength(tmp1);

    err = DbspjErr::QueryNodeTooBig;
    if (unlikely(node_len >= NDB_ARRAY_SIZE(m_buffer0))) {
      jam();
      goto error;
    }

    err = DbspjErr::QueryNodeParametersTooBig;
    if (unlikely(param_len >= NDB_ARRAY_SIZE(m_buffer1))) {
      jam();
      goto error;
    }

    err = DbspjErr::InvalidTreeNodeSpecification;
    if (unlikely(tree.getWords(m_buffer0, node_len) == false)) {
      jam();
      goto error;
    }

    err = DbspjErr::InvalidTreeParametersSpecification;
    if (unlikely(param.getWords(m_buffer1, param_len) == false)) {
      jam();
      goto error;
    }

#if defined(DEBUG_LQHKEYREQ) || defined(DEBUG_SCAN_FRAGREQ)
    printf("node: ");
    for (Uint32 i = 0; i < node_len; i++) printf("0x%.8x ", m_buffer0[i]);
    printf("\n");

    printf("param: ");
    for (Uint32 i = 0; i < param_len; i++) printf("0x%.8x ", m_buffer1[i]);
    printf("\n");
#endif

    err = DbspjErr::UnknowQueryOperation;
    if (unlikely(node_op != param_op)) {
      jam();
      goto error;
    }
    if (ERROR_INSERTED_CLEAR(17006)) {
      g_eventLogger->info(
          "Injecting UnknowQueryOperation error 17006 at line %d file %s",
          __LINE__, __FILE__);
      jam();
      goto error;
    }

    const OpInfo *info = NULL;
    if (unlikely(node_op == QueryNode::QN_SCAN_FRAG_v1)) {
      /**
       * Convert the deprecated SCAN_FRAG_v1 node+param to new SCAN_FRAG:
       *  - The 'node' formats are identical, no conversion needed.
       *  - The QN_ScanFragParameters has two additional 'batch_size' members.
       *    In addition there is three unused Uint32 member for future use. (5)
       *    Extend entire param block to make room for it, fill in from 'req'.
       *
       *    {len, requestInfo, resultData}
       *     -> {len, requestInfo, resultData,
       *         batch_size_rows, batch_size_bytes, unused0-2}
       */
      jam();
      QN_ScanFragParameters_v1 *param_old =
          (QN_ScanFragParameters_v1 *)m_buffer1;
      const Uint32 requestInfo = param_old->requestInfo;
      const Uint32 resultData = param_old->resultData;

      if (unlikely(param_len + 5 >= NDB_ARRAY_SIZE(m_buffer1))) {
        jam();
        err = DbspjErr::QueryNodeParametersTooBig;
        goto error;
      }
      QN_ScanFragParameters *param = (QN_ScanFragParameters *)m_buffer1;
      /* Moving data beyond 'NodeSize' after the space for new parameters */
      memmove(((Uint32 *)param) + param->NodeSize,
              ((Uint32 *)param_old) + param_old->NodeSize,
              (param_len - param_old->NodeSize) * sizeof(Uint32));
      param_len += 5;

      param->requestInfo = requestInfo;
      param->resultData = resultData;

      /* Calculate and fill in param 'batchSize' from request */
      Signal *signal = ctx.m_start_signal;
      const ScanFragReq *req = (const ScanFragReq *)(signal->getDataPtr());
      param->batch_size_rows = req->batch_size_rows;
      param->batch_size_bytes = req->batch_size_bytes;
      param->unused0 = 0;
      param->unused1 = 0;
      param->unused2 = 0;

      /* Execute root scan with full parallelism - as SCAN_FRAG_v1 always did */
      param->requestInfo |= QN_ScanFragParameters::SFP_PARALLEL;

      info = &Dbspj::g_ScanFragOpInfo;
    } else if (unlikely(node_op == QueryNode::QN_SCAN_INDEX_v1)) {
      /**
       * Convert the deprecated SCAN_INDEX_v1 node+param to new SCAN_FRAG:
       *  - The 'node' formats are identical, no conversion needed.
       *  - The QN_ScanIndexParameters has split the single batchSize into
       *    two separate 'batch_size' members and introduced an additional
       *    three unused Uint32 members for future use. (Total 4)
       *    Extend entire param block to make room for it,
       *    fill in from old batchSize argument.
       *
       *    {len, requestInfo, batchSize, resultData}
       *     -> {len, requestInfo, resultData,
       *         batch_size_rows, batch_size_bytes, unused0-2}
       */
      jam();
      QN_ScanIndexParameters_v1 *param_old =
          (QN_ScanIndexParameters_v1 *)m_buffer1;
      const Uint32 requestInfo = param_old->requestInfo;
      const Uint32 batchSize = param_old->batchSize;
      const Uint32 resultData = param_old->resultData;

      if (unlikely(param_len + 4 >= NDB_ARRAY_SIZE(m_buffer1))) {
        jam();
        err = DbspjErr::QueryNodeParametersTooBig;
        goto error;
      }
      QN_ScanFragParameters *param = (QN_ScanFragParameters *)m_buffer1;
      /* Moving data beyond 'NodeSize' after the space for new parameters */
      memmove(((Uint32 *)param) + param->NodeSize,
              ((Uint32 *)param_old) + param_old->NodeSize,
              (param_len - param_old->NodeSize) * sizeof(Uint32));
      param_len += 4;

      param->requestInfo = requestInfo;
      param->resultData = resultData;
      param->batch_size_rows =
          batchSize & ~(0xFFFFFFFF << QN_ScanIndexParameters_v1::BatchRowBits);
      param->batch_size_bytes =
          batchSize >> QN_ScanIndexParameters_v1::BatchRowBits;
      param->unused0 = 0;
      param->unused1 = 0;
      param->unused2 = 0;

      info = &Dbspj::g_ScanFragOpInfo;
    } else {
      info = getOpInfo(node_op);
      if (unlikely(info == NULL)) {
        jam();
        goto error;
      }
    }

    QueryNode *qn = (QueryNode *)m_buffer0;
    QueryNodeParameters *qp = (QueryNodeParameters *)m_buffer1;
    qn->len = node_len;
    qp->len = param_len;
    err = (this->*(info->m_build))(ctx, requestPtr, qn, qp);
    if (unlikely(err != 0)) {
      jam();
      goto error;
    }

    /**
     * only first node gets access to signal
     */
    ctx.m_start_signal = NULL;

    ndbrequire(ctx.m_cnt < NDB_ARRAY_SIZE(ctx.m_node_list));
    ctx.m_cnt++;
  }
  requestPtr.p->m_node_cnt = ctx.m_cnt;

  if (ctx.m_scan_cnt > 1) {
    jam();
    requestPtr.p->m_bits |= Request::RT_MULTI_SCAN;
  }

  // Set up the order of execution plan
  buildExecPlan(requestPtr);

  // Construct RowBuffers where required
  err = initRowBuffers(requestPtr);
  if (unlikely(err != 0)) {
    jam();
    goto error;
  }

  return 0;

error:
  jam();
  return err;
}

/**
 * initRowBuffers will decide row-buffering strategy, and init
 * the RowBuffers where required.
 */
Uint32 Dbspj::initRowBuffers(Ptr<Request> requestPtr) {
  /**
   * Init BUFFERS in case TreeNode need to buffer any rows/matches
   */
  Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
  Ptr<TreeNode> treeNodePtr;
  for (list.first(treeNodePtr); !treeNodePtr.isNull(); list.next(treeNodePtr)) {
    ndbassert(treeNodePtr.p->m_batch_size > 0);
    /**
     * Construct the local treeNode RowBuffer allocator.
     * As the RowBuffers are pr TreeNode, entire buffer area
     * may be released, instead of releasing row by row.
     */
    treeNodePtr.p->m_rowBuffer.init();

    /**
     * Construct a List or Map RowCollection for those TreeNodes
     * requiring rows to be buffered.
     */
    if (treeNodePtr.p->m_bits & TreeNode::T_BUFFER_MAP) {
      jam();
      treeNodePtr.p->m_rows.construct(RowCollection::COLLECTION_MAP,
                                      treeNodePtr.p->m_rowBuffer,
                                      MaxCorrelationId);
    } else {
      jam();
      treeNodePtr.p->m_rows.construct(RowCollection::COLLECTION_LIST,
                                      treeNodePtr.p->m_rowBuffer,
                                      MaxCorrelationId);
    }
  }

  return 0;
}  // Dbspj::initRowBuffers

/**
 * setupAncestors():
 *
 * Complete the query tree topology as given by the SPJ API:
 *
 * Fill in the m_ancestors bitMask, and set the reference to
 * our closest scanAncestor in each TreeNode. Also set
 * the 'm_coverage' of each TreeNode.
 */
void Dbspj::setupAncestors(Ptr<Request> requestPtr, Ptr<TreeNode> treeNodePtr,
                           Uint32 scanAncestorPtrI) {
  LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                             m_dependency_map_pool);
  Local_dependency_map const childList(pool, treeNodePtr.p->m_child_nodes);
  Dependency_map::ConstDataBufferIterator it;

  treeNodePtr.p->m_scanAncestorPtrI = scanAncestorPtrI;
  if (treeNodePtr.p->isScan()) {
    scanAncestorPtrI = treeNodePtr.i;
  }

  for (childList.first(it); !it.isNull(); childList.next(it)) {
    jam();
    Ptr<TreeNode> childPtr;
    ndbrequire(m_treenode_pool.getPtr(childPtr, *it.data));

    childPtr.p->m_ancestors = treeNodePtr.p->m_ancestors;
    childPtr.p->m_ancestors.set(treeNodePtr.p->m_node_no);

    setupAncestors(requestPtr, childPtr, scanAncestorPtrI);

    treeNodePtr.p->m_coverage.bitOR(childPtr.p->m_coverage);
  }
  treeNodePtr.p->m_coverage.set(treeNodePtr.p->m_node_no);
}

/**
 * buildExecPlan()
 *
 *   Decides the order/pace in which the different TreeNodes should
 *   be executed. We basically choose between two strategies:
 *
 *   Lookup-queries returns at most a single row from each
 *   TreeNode in the SPJ-request. We believe these to impose
 *   a relatively low CPU load on the system. We try to reduce
 *   the elapsed execution time for these requests by
 *   submitting as many of the LQHKEYREQ's as possible in parallel.
 *   Thereby also taking advantage of the datanode parallelism.
 *
 *   On the other hand, scan queries has the potential for returning
 *   huge result sets. Furthermore, the root scan operation will
 *   result is SPJ sub requests being sent to all datanodes. Thus
 *   the datanode parallelism is utilized without executing
 *   the SPJ requests TreeNodes in parallel. For such queries
 *   we will execute INNER-joined TreeNodes in sequence, wherever
 *   possible taking advantage of that we can skip further operations
 *   on rows where preceding matches were not found.
 *
 *   Note that prior to introducing INNER-join handling in SPJ,
 *   all queries effectively were executed with the most parallel
 *   execution plan.
 */
Uint32 Dbspj::buildExecPlan(Ptr<Request> requestPtr) {
  Ptr<TreeNode> treeRootPtr;
  Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
  list.first(treeRootPtr);

  setupAncestors(requestPtr, treeRootPtr, RNIL);

  if (requestPtr.p->isScan()) {
    const Uint32 err =
        planSequentialExec(requestPtr, treeRootPtr, NullTreeNodePtr);
    if (unlikely(err)) return err;
  } else {
    const Uint32 err = planParallelExec(requestPtr, treeRootPtr);
    if (unlikely(err)) return err;
  }

#ifdef VM_TRACE
  DEBUG("Execution plan, TreeNode execution order:");
  dumpExecPlan(requestPtr, treeRootPtr);
#endif

  return 0;
}  // buildExecPlan()

/**
 * planParallelExec():
 *
 *  Set up the most parallelized execution plan for the query.
 *  This happens to be the same query topology as represented by the
 *  child / parent references represented in SPJ request from the API.
 *  So we could simply copy the child / ancestor dependencies as
 *  the final order of execution.
 *
 *  For such an execution plan we may execute all child-TreeNodes in
 *  parallel - Even if there are non-matching child rows which will
 *  eventually result in both the parent row, and all adjacent child rows
 *  to be eliminated from a final inner-joined result set.
 *
 *  Such a join plan is most suited for a query processing relatively few
 *  rows, where the overhead of returning rows which are later eliminated
 *  is low. The possible advantage if this query plan is a lower elapsed time
 *  for the query execution, possible at the cost of some higher CPU usage.
 */
Uint32 Dbspj::planParallelExec(Ptr<Request> requestPtr,
                               Ptr<TreeNode> treeNodePtr) {
  LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                             m_dependency_map_pool);
  Local_dependency_map child(pool, treeNodePtr.p->m_child_nodes);
  Local_dependency_map execList(pool, treeNodePtr.p->m_next_nodes);
  Dependency_map::ConstDataBufferIterator it;

  treeNodePtr.p->m_predecessors = treeNodePtr.p->m_ancestors;
  treeNodePtr.p->m_dependencies = treeNodePtr.p->m_ancestors;

  for (child.first(it); !it.isNull(); child.next(it)) {
    Ptr<TreeNode> childPtr;
    ndbrequire(m_treenode_pool.getPtr(childPtr, *it.data));
    if (unlikely(!execList.append(&childPtr.i, 1))) {
      jam();
      return DbspjErr::OutOfQueryMemory;
    }

    const Uint32 err = planParallelExec(requestPtr, childPtr);
    if (unlikely(err)) return err;

    treeNodePtr.p->m_coverage.bitOR(childPtr.p->m_coverage);
  }

  return 0;
}  // Dbspj::planParallelExec

/**
 * planSequentialExec()
 *
 *   Build an execution plan where INNER-joined TreeNodes are executed in
 *   sequence, such that further evaluation of non-matching rows could be
 *   skipped as early as possible.
 *
 *  Steps:
 *
 * 1)
 *  Each 'branch' has the property that it starts with either a scan-TreeNode,
 *  or an outer joined TreeNode. Any INNER-joined lookup-nodes having
 *  this TreeNode as a (grand-)parent, is also a member of the branch.
 *
 *  Such a 'branch' of INNER-joined lookups has the property that an EQ-match
 *  has to be found from all its TreeNodes in order for any of the related
 *  rows to be part of the joined result set. Thus, during execution we can
 *  skip any further child lookups as soon as a non-match is found. This is
 *  represented in the execution plan by appending the INNER-joined lookups
 *  in a sequence.
 *
 *  Note that we are 'greedy' in appending these INNER-joined lookups,
 *  such that a lookup-TreeNode may effectively be executed prior to a
 *  scan-TreeNode, even if the scan is located before the lookup in the
 *  'm_nodes' list produced by the SPJ-API. This is intentional as a
 *  potential non-matching lookup row would eliminate the need for
 *  executing the much more expensive (index-)scan operation.
 *
 * 2)
 *  Recursively append a *single* INNER-joined scan-*branch* after the
 *  end of the branch from 1). As it is called recursively, the scan
 *  branch will append further lookup-nodes which depended on this scan-node,
 *  and finally append any remaining INNER-joined scan branches.
 *
 *  Note1 that due to old legacy in the SPJ-API protocol, all scan nodes
 *  has to be executed in order relative to each other. (Explains the 'single'
 *  mentioned above)
 *
 *  Note2: After the two steps above has completed, including the recursive call
 *  handling the INNER-joined scan, all INNER-joined TreeNodes to be joined with
 *  this 'branch' have been added to the exec plan.
 *
 *  Note3: Below we use the term 'non-INNER-joined', instead of 'OUTER-joined'.
 *  This is due to SPJ-API protocol compatibility, where we previously didn't
 *  tag the TreeNodes as being INNER-joined or not. Thus when receiving a SPJ
 *  request from an API client, we can't tell for sure whether the TreeNode
 *  is outer joined, or if the (old) client simply didn't specify INNER-joins.
 *  Thus all we know is that nodes are 'non-INNER-joined'.
 *
 *  Also note that for any request from such an old API client, there will
 *  not be appended any 'sequential' TreeNodes to the exec plan in 1) and 2)
 *  above. Only steps 3) and 4) below will effectively be used, which will
 *  (intentionally) result in a parallelized query plan, identical to what
 *  it used to be prior to introducing these INNER-join optimizations.
 *
 * 3)
 *  Recursively append all non-INNER-joined branches to be executed in
 *  *parallel* with each other - after the sequence of INNER-joins. (from 1+2)
 */
Uint32 Dbspj::planSequentialExec(Ptr<Request> requestPtr,
                                 const Ptr<TreeNode> branchPtr,
                                 Ptr<TreeNode> prevExecPtr,
                                 TreeNodeBitMask outerJoins) {
  DEBUG("planSequentialExec, start branch at treeNode no: "
        << branchPtr.p->m_node_no);

  // Append head of branch to be executed after 'prevExecPtr'
  const Uint32 err = appendTreeNode(requestPtr, branchPtr, prevExecPtr);
  if (unlikely(err)) return err;

  Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
  TreeNodeBitMask predecessors(branchPtr.p->m_predecessors);
  predecessors.set(branchPtr.p->m_node_no);

  // In case we enter a new outer-joined-nest, add it to our recursed context
  if ((branchPtr.p->m_bits & TreeNode::T_INNER_JOIN) == 0)
    outerJoins.set(branchPtr.p->m_node_no);

  /**
   * 1) Append all INNER-joined lookups to the 'plan' to be executed in
   * sequence. Maintain the set of 'predecessor' TreeNodes which are already
   * executed. Don't append TreeNodes where its ancestors are not part of the
   * 'plan'
   */
  Ptr<TreeNode> treeNodePtr(branchPtr);
  prevExecPtr = treeNodePtr;
  while (list.next(treeNodePtr)) {
    if (treeNodePtr.p->m_predecessors.isclear() &&
        predecessors.contains(treeNodePtr.p->m_ancestors) &&
        treeNodePtr.p->m_bits & TreeNode::T_INNER_JOIN &&
        treeNodePtr.p->isLookup()) {
      DEBUG("planSequentialExec, append INNER-join lookup treeNode: "
            << treeNodePtr.p->m_node_no
            << ", to branch at: " << branchPtr.p->m_node_no
            << ", as 'descendant' of node: " << prevExecPtr.p->m_node_no);

      // Add INNER-joined lookup treeNode to the join plan:
      const Uint32 err = appendTreeNode(requestPtr, treeNodePtr, prevExecPtr);
      if (unlikely(err)) return err;

      predecessors.set(treeNodePtr.p->m_node_no);
      prevExecPtr = treeNodePtr;
    }
  }  // for 'all request TreeNodes', starting from branchPtr

  /**
   * 2) After this INNER-joined lookup sequence:
   * Recursively append a *single* INNER-joined scan-branch, if found.
   *
   * Note that this branch, including any non-INNER joined branches below,
   * are planned to be executed in *parallel* after the 'prevExecPtr',
   * which is the end of the sequence of INNER-lookups.
   */
  treeNodePtr = branchPtr;  // Start over
  while (list.next(treeNodePtr)) {
    /**
     * Scan has to be executed in same order as found in the
     * list of TreeNodes. (Legacy of the original SPJ-API result protocol)
     */
    if (treeNodePtr.p->m_predecessors.isclear() &&
        predecessors.contains(treeNodePtr.p->m_ancestors) &&
        treeNodePtr.p->m_bits & TreeNode::T_INNER_JOIN) {
      DEBUG("planSequentialExec, append INNER-joined scan-branch at: "
            << treeNodePtr.p->m_node_no);

      ndbassert(treeNodePtr.p->isScan());
      const Uint32 err =
          planSequentialExec(requestPtr, treeNodePtr, prevExecPtr, outerJoins);
      if (unlikely(err)) return err;
      break;
    }
  }  // for 'all request TreeNodes', starting from branchPtr

  /**
   * Note: All INNER-Joins within current 'branch' will now have been handled,
   * either directly within this method at 1), or by recursively calling it in
   * 2).
   *
   * 3) Append the OUTER-joined branches to be executed after any INNER-joined
   * tables, taking advantage of that any non-matches in the INNER-joins may
   * eliminate the need for executing the entire OUTER-branch as well.
   *
   * Note: '->m_ancestors.contains(outerJoins)'
   * We need to take care to *not* add nodes which are not inside the
   * outer joined nests we have recursed into. We need to pop back to the
   * correct join-nest context before these can be added.
   */
  treeNodePtr = branchPtr;  // Start over
  while (list.next(treeNodePtr)) {
    if (treeNodePtr.p->m_predecessors.isclear() &&
        predecessors.contains(treeNodePtr.p->m_ancestors) &&
        treeNodePtr.p->m_ancestors.contains(outerJoins)) {
      DEBUG("planSequentialExec, append non-INNER-joined branch at: "
            << treeNodePtr.p->m_node_no
            << ", to branch at: " << branchPtr.p->m_node_no
            << ", as 'descendant' of node: " << prevExecPtr.p->m_node_no);

      // A non-INNER joined TreeNode
      ndbassert((treeNodePtr.p->m_bits & TreeNode::T_INNER_JOIN) == 0);
      const Uint32 err =
          planSequentialExec(requestPtr, treeNodePtr, prevExecPtr, outerJoins);
      if (unlikely(err)) return err;
    }
  }  // for 'all request TreeNodes', starting from branchPtr

  return 0;
}  // ::planSequentialExec

/**
 * appendTreeNode()
 *
 *  Appends 'treeNodePtr' to the execution plan after 'prevExecPtr'.
 *
 *  Fills in the 'predecessors' and 'dependencies' bitmask.
 *
 *  Sets of extra 'scheduling policy' described by 'm_resumeEvents',
 *  and BUFFERing of rows and/or their match bitmask
 *  as required by the chosen scheduling.
 */
Uint32 Dbspj::appendTreeNode(Ptr<Request> requestPtr, Ptr<TreeNode> treeNodePtr,
                             Ptr<TreeNode> prevExecPtr) {
  if (prevExecPtr.isNull()) {
    // Is root, assert that no further action would have been required below.
    ndbassert(treeNodePtr.p->m_parentPtrI == RNIL);
    ndbassert(treeNodePtr.p->m_scanAncestorPtrI == RNIL);
    return 0;
  }

  DEBUG("appendTreeNode, append treeNode: " << treeNodePtr.p->m_node_no
                                            << ", as 'descendant' of node: "
                                            << prevExecPtr.p->m_node_no);
  {
    LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                               m_dependency_map_pool);

    // Add treeNode to the execution plan:
    Local_dependency_map execList(pool, prevExecPtr.p->m_next_nodes);
    if (unlikely(!execList.append(&treeNodePtr.i, 1))) {
      jam();
      return DbspjErr::OutOfQueryMemory;
    }
  }

  treeNodePtr.p->m_predecessors.bitOR(prevExecPtr.p->m_predecessors);
  treeNodePtr.p->m_predecessors.set(prevExecPtr.p->m_node_no);

  treeNodePtr.p->m_dependencies = prevExecPtr.p->m_dependencies;
  treeNodePtr.p->m_dependencies.set(prevExecPtr.p->m_node_no);

  ndbassert(
      treeNodePtr.p->m_predecessors.contains(treeNodePtr.p->m_dependencies));
  ndbassert(treeNodePtr.p->m_dependencies.contains(treeNodePtr.p->m_ancestors));

  /**
   * Below we set up any special scheduling policy.
   *
   * If nothing is set, completion of a request will submit new request(s) for
   * all 'm_next_nodes' in *parallel*. The result rows returned from the request
   * will be used directly as the 'parentRow' to produce the new request(s).
   *
   * So anything set up below is an exception to this basic rule!
   */

  /**
   * Job-buffer congestion control:
   *
   * A large number of rows may be returned in each scanBatch, each of them
   * resulting in a LQHKEYREQ being sent to any lookup child-TreeNodes.
   * The congestion control aims to limit the number of such LQHKEYREQs
   * to be 'outstanding'. Thus, all scan-TreeNodes having lookup children
   * should have T_CHK_CONGESTION set in order to activate the congestion
   * control. As T_CHK_CONGESTION may storeRow() on the scan-TreeNode,
   * later being looked up from its correlation-id, T_BUFFER_MAP is also
   * needed for such buffered rows.
   */
  if (treeNodePtr.p->isLookup() && prevExecPtr.p->isScan()) {
    prevExecPtr.p->m_bits |= TreeNode::T_CHK_CONGESTION;
    prevExecPtr.p->m_bits |= TreeNode::T_BUFFER_MAP;
  }

  /**    Example:
   *
   *       scan1
   *       /   \      ====INNER-join executed as===>  scan1 -> scan2 -> op3
   *    scan2  op3(scan or lookup)
   *
   * Considering case above, both scan2 and op3 has scan1 as its scanAncestor.
   * In an INNER-joined execution plan, we will take advantage of that
   * a match between scan1 join scan2 rows are required, else 'join op3' could
   * be skipped. Thus, even if scan1 is the scan-ancestor of op3, we will
   * execute scan2 in between these.
   *
   * Note that the result from scan2 may have multiple TRANSID_AI results
   * returned for each row from scan1. Thus we can't directly use the returned
   * scan2 rows to trigger production of the op3 requests. (Due to cardinality
   * mismatch). The op3 requests has to be produced based on scan1 results!
   *
   * We set up the scheduling policy below to solve this:
   * - TN_EXEC_WAIT is set on 'op3', which will prevent TRANSID_AI
   *     results from scan2 from submiting operations to op3.
   * - TN_RESUME_NODE is set on 'op3' which will result in
   *     ::resumeBufferedNode() being called when all TreeNodes
   *     which we depends in has completed their batches.
   *     (Also implies that the parent of any to-be-resumed-nodes
   *      need to BUFFER_ROW).
   *
   * ::resumeBufferedNode() will iterate all its buffered parent results.
   * For each row we will check if the required INNER-join matches from
   * the TreeNodes it has INNER-join dependencies on. Non-matching parent
   * rows are skipped from further requests.
   *
   * We maintain the found matches in the m_match-bitmask in the
   * BUFFER structure of each TreeNode scanAncestor. Below we set
   * the T_BUFFER_MATCH on the scanAncestor, and all scans in between
   * in order to having the match-bitmap being set up.
   */
  if (treeNodePtr.p->m_scanAncestorPtrI != RNIL) {
    Ptr<TreeNode> scanAncestorPtr;
    ndbrequire(m_treenode_pool.getPtr(scanAncestorPtr,
                                      treeNodePtr.p->m_scanAncestorPtrI));
    Ptr<TreeNode> ancestorPtr(scanAncestorPtr);

    // Note that scans are always added to exec plan such that their
    // relative order is kept.

    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    while (list.next(ancestorPtr) && ancestorPtr.i != treeNodePtr.i) {
      if (ancestorPtr.p->isScan() &&
          treeNodePtr.p->m_dependencies.get(ancestorPtr.p->m_node_no)) {
        /**
         * 'ancestorPtr' is a scan executed in between this scan and its
         * scanAncestor. It is not among the ancestors of the TreeNode to be
         * executed
         */

        // Need 'resume-node' scheduling in preparation for 'next' scan-branch:
        treeNodePtr.p->m_resumeEvents |=
            TreeNode::TN_EXEC_WAIT | TreeNode::TN_RESUME_NODE;

        scanAncestorPtr.p->m_bits |=
            TreeNode::T_BUFFER_MAP | TreeNode::T_BUFFER_MATCH;

        /**
         * BUFFER_MATCH all scan ancestors of this treeNode which we
         * depends on (May exclude some outer-joined scan branches.)
         */
        if (!ancestorPtr.p->isLeaf()) {
          ancestorPtr.p->m_bits |=
              TreeNode::T_BUFFER_MAP | TreeNode::T_BUFFER_MATCH;
        }
      }
    }

    const bool pruned = treeNodePtr.p->m_bits &
                        (TreeNode::T_PRUNE_PATTERN | TreeNode::T_CONST_PRUNE);
    const bool leafAndFirstMatch =
        treeNodePtr.p->isLeaf() &&
        (treeNodePtr.p->m_bits & TreeNode::T_FIRST_MATCH);
    if (leafAndFirstMatch && !pruned) {
      /**
       * firstMatch execution 'REDUCE_KEYS' to remove already found matches.
       * Only relevant if not 'pruned', else there are different keys sent
       * to each node.
       *
       * Ancestor of the firstMatched scan-node need to keep track of which
       * range-keys it found matches for, and how that row MAP'ed to its
       * ancestors.
       */
      treeNodePtr.p->m_bits |= TreeNode::T_REDUCE_KEYS;
      scanAncestorPtr.p->m_bits |=
          TreeNode::T_BUFFER_MAP | TreeNode::T_BUFFER_MATCH;
    }
  }

  /**
   * Only the result rows from the 'prevExec' is directly available when
   * operations for this TreeNode is scheduled. If that is not the parent
   * of this TreeNode, we have to BUFFER the parent rows such that
   * they can be looked up by the correlationId when needed. NOTE, that
   * all Lookup result rows having the same scanAncestor, will also
   * share the same correlationId as their scanAncestor. Such that the
   * correlationId from a prevExec result row, may be used to
   * BUFFER_MAP-locate the related parent rows.
   *
   * Also take care of buffering parent rows for enqueued ops and
   * to-be-resumed nodes, as described above.
   */
  if (treeNodePtr.p->m_parentPtrI != prevExecPtr.i ||
      (treeNodePtr.p->m_resumeEvents & TreeNode::TN_RESUME_NODE)) {
    /**
     * As execution of this tree branch is not initiated by
     * its own parent, we need to buffer the parent rows
     * such that they can be located when needed.
     */
    Ptr<TreeNode> parentPtr;
    ndbrequire(m_treenode_pool.getPtr(parentPtr, treeNodePtr.p->m_parentPtrI));
    parentPtr.p->m_bits |= TreeNode::T_BUFFER_MAP | TreeNode::T_BUFFER_ROW;
  }

  return 0;
}

void Dbspj::dumpExecPlan(Ptr<Request> requestPtr, Ptr<TreeNode> treeNodePtr) {
  LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                             m_dependency_map_pool);
  const Local_dependency_map nextExec(pool, treeNodePtr.p->m_next_nodes);
  Dependency_map::ConstDataBufferIterator it;

  DEBUG("TreeNode no: "
        << treeNodePtr.p->m_node_no
        << ", coverage are: " << treeNodePtr.p->m_coverage.rep.data[0]
        << ", ancestors are: " << treeNodePtr.p->m_ancestors.rep.data[0]
        << ", predecessors are: " << treeNodePtr.p->m_predecessors.rep.data[0]
        << ", depending on: " << treeNodePtr.p->m_dependencies.rep.data[0]);

  if (treeNodePtr.p->isLookup()) {
    DEBUG("  'Lookup'-node");
  } else if (treeNodePtr.p->isScan()) {
    DEBUG("  '(Index-)Scan'-node");
  }

  if (treeNodePtr.p->m_parentPtrI != RNIL) {
    if (treeNodePtr.p->m_bits & TreeNode::T_INNER_JOIN) {
      DEBUG("  INNER_JOIN");
    } else if (treeNodePtr.p->m_parentPtrI != RNIL) {
      DEBUG("  OUTER_JOIN");
    }
    if (treeNodePtr.p->m_bits & TreeNode::T_FIRST_MATCH) {
      DEBUG("  FIRST_MATCH");
    }
  }

  if (treeNodePtr.p->m_resumeEvents & TreeNode::TN_EXEC_WAIT) {
    DEBUG("  has EXEC_WAIT");
  }
  if (treeNodePtr.p->m_resumeEvents & TreeNode::TN_RESUME_NODE) {
    DEBUG("  has RESUME_NODE");
  }

  static const Uint32 BufferAll =
      (TreeNode::T_BUFFER_ROW | TreeNode::T_BUFFER_MATCH);
  if ((treeNodePtr.p->m_bits & BufferAll) == BufferAll) {
    DEBUG("  BUFFER 'ROWS'+'MATCH'");
  } else if (treeNodePtr.p->m_bits & TreeNode::T_BUFFER_ROW) {
    DEBUG("  BUFFER 'ROWS'");
  } else if (treeNodePtr.p->m_bits & TreeNode::T_BUFFER_MATCH) {
    DEBUG("  BUFFER 'MATCH'");
  }
  if (treeNodePtr.p->m_bits & TreeNode::T_CHK_CONGESTION) {
    DEBUG("  BUFFER 'ROWS' If 'congested'");
  }

  for (nextExec.first(it); !it.isNull(); nextExec.next(it)) {
    Ptr<TreeNode> nextPtr;
    ndbrequire(m_treenode_pool.getPtr(nextPtr, *it.data));
    DEBUG("  TreeNode no: " << treeNodePtr.p->m_node_no
                            << ", has nextExec: " << nextPtr.p->m_node_no);
  }

  for (nextExec.first(it); !it.isNull(); nextExec.next(it)) {
    Ptr<TreeNode> nextPtr;
    ndbrequire(m_treenode_pool.getPtr(nextPtr, *it.data));
    dumpExecPlan(requestPtr, nextPtr);
  }
}

Uint32 Dbspj::createNode(Build_context &ctx, Ptr<Request> requestPtr,
                         Ptr<TreeNode> &treeNodePtr) {
  /**
   * In the future, we can have different TreeNode-allocation strategies
   *   that can be setup using the Build_context
   *
   */
  if (ERROR_INSERTED_CLEAR(17005)) {
    g_eventLogger->info(
        "Injecting OutOfOperations error 17005 at line %d file %s", __LINE__,
        __FILE__);
    jam();
    return DbspjErr::OutOfOperations;
  }
  if (m_treenode_pool.seize(requestPtr.p->m_arena, treeNodePtr)) {
    DEBUG("createNode - seize -> ptrI: " << treeNodePtr.i);
    new (treeNodePtr.p) TreeNode(requestPtr.i);
    ctx.m_node_list[ctx.m_cnt] = treeNodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    list.addLast(treeNodePtr);
    treeNodePtr.p->m_node_no = ctx.m_cnt;
    insertGuardedPtr(requestPtr, treeNodePtr);
    return 0;
  }
  return DbspjErr::OutOfOperations;
}

/**
 * Depending on query type, a 'prepare' phase might be required
 * before starting the real data retrieval from the query.
 *
 * All ::exec<FOO> methods handling replies related to the query
 * prepare phase, should call ::checkPrepareComplete() before
 * they return.
 */
void Dbspj::prepare(Signal *signal, Ptr<Request> requestPtr) {
  Uint32 err = 0;
  if (requestPtr.p->m_bits & Request::RT_NEED_PREPARE) {
    jam();
    requestPtr.p->m_outstanding = 0;
    requestPtr.p->m_state = Request::RS_PREPARING;

    Ptr<TreeNode> nodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    for (list.first(nodePtr); !nodePtr.isNull(); list.next(nodePtr)) {
      jam();
      /**
       * Verify existence of all involved tables.
       */
      err = checkTableError(nodePtr);
      if (unlikely(err)) {
        jam();
        break;
      }
      if (nodePtr.p->m_bits & TreeNode::T_NEED_PREPARE) {
        jam();
        ndbassert(nodePtr.p->m_info != NULL);
        ndbassert(nodePtr.p->m_info->m_prepare != NULL);
        (this->*(nodePtr.p->m_info->m_prepare))(signal, requestPtr, nodePtr);
      }
    }

    /**
     * preferably RT_NEED_PREPARE should only be set if blocking
     * calls are used, in which case m_outstanding should have been increased
     */
    ndbassert(err || requestPtr.p->m_outstanding);
  }
  if (unlikely(err)) {
    jam();
    abort(signal, requestPtr, err);
    return;
  }
}

/**
 * Check if all outstanding 'prepare' work has completed.
 * After prepare completion, start the query itself.
 *
 * A prepare completion could also complete the entire request.
 * Thus, checkBatchComplete() is also checked as part of
 * prepare completion.
 */
void Dbspj::checkPrepareComplete(Signal *signal, Ptr<Request> requestPtr) {
  if (requestPtr.p->m_outstanding > 0) {
    return;
  }

  do  // To simplify error/exit handling, no real loop
  {
    jam();
    if (unlikely((requestPtr.p->m_state & Request::RS_ABORTING) != 0)) {
      jam();
      break;
    }

    Ptr<TreeNode> nodePtr;
    {
      Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
      ndbrequire(list.first(nodePtr));
    }
    Uint32 err = checkTableError(nodePtr);
    if (unlikely(err != 0)) {
      jam();
      abort(signal, requestPtr, err);
      break;
    }

    requestPtr.p->m_state = Request::RS_RUNNING;
    ndbrequire(nodePtr.p->m_info != 0 && nodePtr.p->m_info->m_start != 0);
    (this->*(nodePtr.p->m_info->m_start))(signal, requestPtr, nodePtr);
  } while (0);

  // Possibly completed (or failed) entire request.
  checkBatchComplete(signal, requestPtr);
}

/**
 * Check if all outstanding work for 'Request' has completed.
 *
 * All ::exec<FOO> methods handling replies related to query
 * execution, *must* call ::checkBatchComplete() before returning.
 */
void Dbspj::checkBatchComplete(Signal *signal, Ptr<Request> requestPtr) {
  if (unlikely(requestPtr.p->m_outstanding == 0)) {
    jam();
    batchComplete(signal, requestPtr);
  }
}

/**
 * Request has completed all outstanding work.
 * Signal API about completion status and cleanup
 * resources if appropriate.
 *
 * NOTE: A Request might ::batchComplete() twice if
 * a completion phase is required. It will then be called
 * the last time from ::complete()
 */
void Dbspj::batchComplete(Signal *signal, Ptr<Request> requestPtr) {
  ndbrequire(requestPtr.p->m_outstanding ==
             0);  // "definition" of batchComplete

  bool is_complete = requestPtr.p->m_cnt_active == 0;
  bool need_complete_phase = requestPtr.p->m_bits & Request::RT_NEED_COMPLETE;

  if (requestPtr.p->isLookup()) {
    ndbassert(requestPtr.p->m_cnt_active == 0);
  }

  if (!is_complete || (is_complete && need_complete_phase == false)) {
    /**
     * one batch complete, and either
     *   - request not complete
     *   - or not complete_phase needed
     */
    jam();

    if ((requestPtr.p->m_state & Request::RS_ABORTING) != 0) {
      ndbassert(is_complete);
    }

    // Remember the active treeNodes the completed scan returned rows from
    const TreeNodeBitMask activated_tree_nodes(
        requestPtr.p->m_active_tree_nodes);
    prepareNextBatch(signal, requestPtr);

    /**
     * If we completed a T_SORTED_ORDER request, it would have fetched only a
     * single row from the ordered treeRoot, likely leaving lots of unused
     * batch buffers. Instead of waiting for the client to request a NEXTREQ,
     * we may initiate from here instead, iff:
     *  - Fragments scan is not complete and we had no errors
     *  - It is (still) the ORDERED rootTreeNode(==0) we expect results from.
     */
    if (!is_complete && requestPtr.p->m_errCode == 0 &&
        requestPtr.p->m_cnt_active == 1 &&
        requestPtr.p->m_active_tree_nodes.get(0) &&
        requestPtr.p->m_active_tree_nodes.equal(activated_tree_nodes) &&
        requestPtr.p->m_bits & Request::RT_MULTI_SCAN) {
      Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
      Ptr<TreeNode> treeNodePtr;
      list.first(treeNodePtr);
      const Ptr<TreeNode> treeRootPtr(treeNodePtr);
      ndbassert(treeRootPtr.p->m_state == TreeNode::TN_ACTIVE);

      if (treeRootPtr.p->m_bits & TreeNode::T_SORTED_ORDER) {
        /**
         * Is an ORDERED treeNode and a candidate for fetching more.
         * Experiments has shown that it performanve wise is better to stop
         * filling the batch buffers while we could still return the entire
         * set of related child rows. ::estmMaxKeys() estimates how many
         * root-rows we can fetch and still fit the full set of child rows.
         * Note that this is only an optimization, a missed estimate is
         * not critical.
         */
        const double maxKeys = estmMaxKeys(requestPtr, treeRootPtr);
        if (maxKeys >= 2.0)  // Can Fit one more root-row (with margins)
        {
          jam();
          const ScanFragData &data = treeRootPtr.p->m_scanFrag_data;
          const ScanFragReq *org =
              reinterpret_cast<const ScanFragReq *>(data.m_scanFragReq);

          // Prepare to receive more rows from the NEXTREQ
          cleanupBatch(requestPtr, /*done=*/false);

          const Uint32 bs_rows = 1;
          const Uint32 bs_bytes = (org->batch_size_bytes - data.m_totalBytes);
          ndbassert(requestPtr.p->m_rootFragCnt == 1);
          scanFrag_send_NEXTREQ(signal, requestPtr, treeRootPtr, 1, bs_bytes,
                                bs_rows);

          requestPtr.p->m_outstanding++;
          requestPtr.p->m_completed_tree_nodes.clear(treeRootPtr.p->m_node_no);
          return;  // More rows to be fetched -> no sendConf() yet
        }
      }
    }
    sendConf(signal, requestPtr, is_complete);
  } else if (is_complete && need_complete_phase) {
    jam();
    /**
     * run complete-phase
     */
    complete(signal, requestPtr);
    return;
  }

  if (requestPtr.p->m_cnt_active == 0) {
    jam();
    /**
     * Entire Request completed
     */
    constexpr bool in_hash = true;
    cleanup(requestPtr, in_hash);
  } else {
    jam();
    /**
     * Cleanup the TreeNode branches getting another
     * batch of result rows.
     */
    cleanupBatch(requestPtr, /*done=*/true);
  }
}

/**
 * Locate next TreeNode(s) to retrieve more rows from.
 *
 *   Calculate set of the 'm_active_tree_nodes' we will receive from in NEXTREQ.
 *   Add these TreeNodes to the cursor list to be iterated.
 */
void Dbspj::prepareNextBatch(Signal *signal, Ptr<Request> requestPtr) {
  ndbassert(requestPtr.p->m_suspended_tree_nodes.isclear());
  requestPtr.p->m_cursor_nodes.init();
  requestPtr.p->m_active_tree_nodes.clear();
  requestPtr.p->m_suspended_tree_nodes.clear();

  if (requestPtr.p->m_cnt_active == 0) {
    jam();
    return;
  }

  DEBUG("prepareNextBatch, request: " << requestPtr.i);

  if (requestPtr.p->m_bits & Request::RT_REPEAT_SCAN_RESULT) {
    /**
     * If REPEAT_SCAN_RESULT we handle bushy scans by return more *new* rows
     * from only one of the active child scans. If there are multiple
     * bushy scans not being able to return their current result set in
     * a single batch, result sets from the other child scans are repeated
     * until all rows has been returned to the API client.
     *
     * Hence, the cross joined results from the bushy scans are partly
     * produced within the SPJ block on a 'batchsize granularity',
     * and partly is the responsibility of the API-client by iterating
     * the result rows within the current result batches.
     * (Opposed to non-REPEAT_SCAN_RESULT, the client only have to care about
     *  the current batched rows - no buffering is required)
     */
    jam();
    Ptr<TreeNode> nodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);

    /**
     * Locate last 'TN_ACTIVE' TreeNode which is the only one chosen
     * to return more *new* rows.
     */
    for (list.last(nodePtr); !nodePtr.isNull(); list.prev(nodePtr)) {
      if (nodePtr.p->m_state == TreeNode::TN_ACTIVE) {
        jam();
        DEBUG("Will fetch more from 'active' m_node_no: "
              << nodePtr.p->m_node_no);
        /**
         * A later NEXTREQ will request a *new* batch of rows from this
         * TreeNode.
         */
        registerActiveCursor(requestPtr, nodePtr);
        break;
      }
    }

    /**
     *  Restart/repeat other (fragment scan) child batches which:
     *    - Being 'after' nodePtr located above.
     *    - Not being an ancestor of (depends on) any 'active' TreeNode.
     *      (As these scans are started when rows from these parent nodes
     *      arrives.)
     */
    if (!nodePtr.isNull()) {
      jam();
      DEBUG("Calculate 'active', w/ cursor on m_node_no: "
            << nodePtr.p->m_node_no);

      /* Restart any partial fragment-scans after this 'TN_ACTIVE' TreeNode */
      for (list.next(nodePtr); !nodePtr.isNull(); list.next(nodePtr)) {
        jam();
        if (!nodePtr.p->m_predecessors.overlaps(
                requestPtr.p->m_active_tree_nodes)) {
          jam();
          ndbrequire(nodePtr.p->m_state != TreeNode::TN_ACTIVE);
          ndbrequire(nodePtr.p->m_info != 0);
          if (nodePtr.p->m_info->m_parent_batch_repeat != 0) {
            jam();
            (this->*(nodePtr.p->m_info->m_parent_batch_repeat))(
                signal, requestPtr, nodePtr);
          }
        }
        /**
         * Adapt to SPJ-API protocol legacy:
         * 1)
         *   API always assumed that any node having an 'active' node as
         *   ancestor gets a new batch of result rows. So we didn't explicitly
         *   set the 'active' bit for these siblings, as it was implicit.
         *   In addition, we might now have (INNER-join) dependencies outside
         *   of the set of ancestor nodes. If such a dependent node, not being
         *   one of our ancestor, is 'active' it will also re-activate this
         *   TreeNode -> Has to inform the API about that.
         * 2)
         *   API expect that it is the 'internalOpNo' of the **table** which
         *   is used to address the 'active' nodes. In case of UNIQUE_INDEXs
         *   two TreeNodes are generated:
         *    - First a TreeNode::T_UNIQUE_INDEX_LOOKUP acessing the index.
         *    - Then another TreeNode accessing the table.
         *   Thus, if this node is an UNIQUE_INDEX, the node_no of the related
         *   *table* to be set as 'active' is node_no+1 !!
         */
        else if (!nodePtr.p->m_ancestors.overlaps(
                     requestPtr.p->m_active_tree_nodes)) {
          if (nodePtr.p->m_bits & TreeNode::T_UNIQUE_INDEX_LOOKUP)
            requestPtr.p->m_active_tree_nodes.set(nodePtr.p->m_node_no + 1);
          else
            requestPtr.p->m_active_tree_nodes.set(nodePtr.p->m_node_no);
        }
      }
    }     // if (!nodePtr.isNull()
  } else  // not 'RT_REPEAT_SCAN_RESULT'
  {
    /**
     * If not REPEAT_SCAN_RESULT multiple active TreeNodes may return their
     * remaining result simultaneously. In case of bushy-scans, these
     * concurrent result streams are cross joins of each other
     * in SQL terms. In order to produce the cross joined result, it is
     * the responsibility of the API-client to buffer these streams and
     * iterate them to produce the cross join.
     */
    jam();
    Ptr<TreeNode> nodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    TreeNodeBitMask predecessors_of_active;

    for (list.last(nodePtr); !nodePtr.isNull(); list.prev(nodePtr)) {
      /**
       * If we are active (i.e not consumed all rows originating
       *   from parent rows) and we are not in the set of parents
       *   for any active child:
       *
       * Then, this is a position that execSCAN_NEXTREQ should continue
       */
      if (nodePtr.p->m_state == TreeNode::TN_ACTIVE &&
          !predecessors_of_active.get(nodePtr.p->m_node_no)) {
        jam();
        DEBUG("Add 'active' m_node_no: " << nodePtr.p->m_node_no);
        registerActiveCursor(requestPtr, nodePtr);
        predecessors_of_active.bitOR(nodePtr.p->m_predecessors);
      }
    }
  }  // if (RT_REPEAT_SCAN_RESULT)

  DEBUG("Calculated 'm_active_tree_nodes': "
        << requestPtr.p->m_active_tree_nodes.rep.data[0]);
}

void Dbspj::registerActiveCursor(Ptr<Request> requestPtr,
                                 Ptr<TreeNode> treeNodePtr) {
  Uint32 bit = treeNodePtr.p->m_node_no;
  ndbrequire(!requestPtr.p->m_active_tree_nodes.get(bit));
  requestPtr.p->m_active_tree_nodes.set(bit);

  Local_TreeNodeCursor_list list(m_treenode_pool, requestPtr.p->m_cursor_nodes);
#ifdef VM_TRACE
  {
    Ptr<TreeNode> nodePtr;
    for (list.first(nodePtr); !nodePtr.isNull(); list.next(nodePtr)) {
      ndbrequire(nodePtr.i != treeNodePtr.i);
    }
  }
#endif
  list.addFirst(treeNodePtr);
}

void Dbspj::sendConf(Signal *signal, Ptr<Request> requestPtr,
                     bool is_complete) {
  if (requestPtr.p->isScan()) {
    if (unlikely((requestPtr.p->m_state & Request::RS_WAITING) != 0)) {
      jam();
      /**
       * We aborted request ourselves (due to node-failure ?)
       *   but TC haven't contacted us...so we can't reply yet...
       */
      ndbrequire(is_complete);
      ndbrequire((requestPtr.p->m_state & Request::RS_ABORTING) != 0);
      return;
    }

    if (requestPtr.p->m_errCode == 0) {
      jam();
      ScanFragConf *conf =
          reinterpret_cast<ScanFragConf *>(signal->getDataPtrSend());
      conf->senderData = requestPtr.p->m_senderData;
      conf->transId1 = requestPtr.p->m_transId[0];
      conf->transId2 = requestPtr.p->m_transId[1];
      conf->completedOps = requestPtr.p->m_rows;
      conf->fragmentCompleted = is_complete ? 1 : 0;
      conf->total_len = requestPtr.p->m_active_tree_nodes.rep.data[0];

      /**
       * Collect the map of nodes still having more rows to return.
       * Note that this 'activeMask' is returned as part of the
       * extended format of the ScanFragConf signal introduced in wl7636.
       * If returned to a TC node not yet upgraded, the extended part
       * of the ScanFragConf is simply ignored.
       */
      Uint32 activeMask = 0;
      Ptr<TreeNode> treeNodePtr;
      Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);

      for (list.first(treeNodePtr); !treeNodePtr.isNull();
           list.next(treeNodePtr)) {
        if (treeNodePtr.p->m_state == TreeNode::TN_ACTIVE) {
          ndbassert(treeNodePtr.p->m_node_no <= 31);
          activeMask |= (1 << treeNodePtr.p->m_node_no);
        }
      }
      conf->activeMask = activeMask;
      c_Counters.incr_counter(CI_SCAN_BATCHES_RETURNED, 1);
      c_Counters.incr_counter(CI_SCAN_ROWS_RETURNED, requestPtr.p->m_rows);

#ifdef SPJ_TRACE_TIME
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      const NDB_TICKS then = requestPtr.p->m_save_time;
      const Uint64 diff = NdbTick_Elapsed(then, now).microSec();

      requestPtr.p->m_sum_rows += requestPtr.p->m_rows;
      requestPtr.p->m_sum_running += Uint32(diff);
      requestPtr.p->m_cnt_batches++;
      requestPtr.p->m_save_time = now;

      if (is_complete) {
        Uint32 cnt = requestPtr.p->m_cnt_batches;
        g_eventLogger->info(
            "batches: %u avg_rows: %u avg_running: %u avg_wait: %u", cnt,
            (requestPtr.p->m_sum_rows / cnt),
            (requestPtr.p->m_sum_running / cnt),
            cnt == 1 ? 0 : requestPtr.p->m_sum_waiting / (cnt - 1));
      }
#endif

      /**
       * reset for next batch
       */
      requestPtr.p->m_rows = 0;
      if (!is_complete) {
        jam();
        requestPtr.p->m_state |= Request::RS_WAITING;
      }
#ifdef DEBUG_SCAN_FRAGREQ
      g_eventLogger->info("Dbspj::sendConf() sending SCAN_FRAGCONF ");
      printSCAN_FRAGCONF(stdout, signal->getDataPtrSend(), conf->total_len,
                         DBLQH);
#endif
      sendSignal(requestPtr.p->m_senderRef, GSN_SCAN_FRAGCONF, signal,
                 ScanFragConf::SignalLength_ext, JBB);
    } else {
      jam();
      ndbrequire(is_complete);
      ScanFragRef *ref =
          reinterpret_cast<ScanFragRef *>(signal->getDataPtrSend());
      ref->senderData = requestPtr.p->m_senderData;
      ref->transId1 = requestPtr.p->m_transId[0];
      ref->transId2 = requestPtr.p->m_transId[1];
      ref->errorCode = requestPtr.p->m_errCode;

      sendSignal(requestPtr.p->m_senderRef, GSN_SCAN_FRAGREF, signal,
                 ScanFragRef::SignalLength, JBB);
    }
  } else {
    ndbassert(is_complete);
    if (requestPtr.p->m_errCode) {
      jam();
      Uint32 resultRef = getResultRef(requestPtr);
      TcKeyRef *ref = (TcKeyRef *)signal->getDataPtr();
      ref->connectPtr = requestPtr.p->m_senderData;
      ref->transId[0] = requestPtr.p->m_transId[0];
      ref->transId[1] = requestPtr.p->m_transId[1];
      ref->errorCode = requestPtr.p->m_errCode;
      ref->errorData = 0;

      sendTCKEYREF(signal, resultRef, requestPtr.p->m_senderRef);
    }
  }

  if (ERROR_INSERTED(17531)) {
    /**
     * Takes effect for *next* 'long' SPJ signal which will fail
     * to alloc long mem section. Dbspj::execSIGNAL_DROPPED_REP()
     * will then be called, which is what we intend to test here.
     */
    jam();
    ErrorSignalReceive = DBSPJ;
    ErrorMaxSegmentsToSeize = 1;
  }
}

Uint32 Dbspj::getResultRef(Ptr<Request> requestPtr) {
  Ptr<TreeNode> nodePtr;
  Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
  for (list.first(nodePtr); !nodePtr.isNull(); list.next(nodePtr)) {
    if (nodePtr.p->isLookup()) {
      jam();
      return nodePtr.p->m_lookup_data.m_api_resultRef;
    }
  }
  ndbabort();
  return 0;
}

/**
 * Cleanup resources allocated while fetching last batch, and prepare for more
 * rows to be returned from the still 'm_active_tree_nodes', or nodes having
 * them as parents. If not 'done' this batch was a sub-batch withing the current
 * REQuest - SPJ will request more batches before a sendConf() to the client.
 * When 'done' a SCAN_NEXTREQ is required to fetch a new batch of rows.
 */
void Dbspj::cleanupBatch(Ptr<Request> requestPtr, bool done) {
  /**
   * Needs to be at least 1 active otherwise we should have
   *   taken the Request cleanup "path" in batchComplete
   */
  ndbassert(requestPtr.p->m_cnt_active >= 1);

  Ptr<TreeNode> treeNodePtr;
  Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);

  for (list.first(treeNodePtr); !treeNodePtr.isNull(); list.next(treeNodePtr)) {
    /**
     * Release and re-init row buffer structures for those treeNodes getting
     * more rows in the following NEXTREQ, including all its children.
     */
    if (requestPtr.p->m_active_tree_nodes.get(treeNodePtr.p->m_node_no) ||
        requestPtr.p->m_active_tree_nodes.overlaps(
            treeNodePtr.p->m_predecessors)) {
      // Release rowBuffers used by this TreeNode.
      jam();
      releasePages(treeNodePtr.p->m_rowBuffer);
      treeNodePtr.p->m_rows.init();
    }

    /* Clear parents 'm_matched' bit for all buffered rows: */
    if (treeNodePtr.p->m_bits & TreeNode::T_BUFFER_MATCH) {
      RowIterator iter;
      for (first(treeNodePtr.p->m_rows, iter); !iter.isNull(); next(iter)) {
        jam();
        RowPtr row;
        setupRowPtr(treeNodePtr, row, iter.m_base.m_row_ptr);

        row.m_matched->bitANDC(requestPtr.p->m_active_tree_nodes);
      }
    }

    /**
     * Do further cleanup in treeNodes having predecessors getting more rows.
     * (Which excludes the restarted treeNode itself)
     */
    if (requestPtr.p->m_active_tree_nodes.overlaps(
            treeNodePtr.p->m_predecessors)) {
      jam();
      /**
       * Common TreeNode cleanup:
       * Deferred operations will have correlation ids which may refer
       * buffered rows released above. These are allocated in
       * the m_batchArena released below.
       * As an optimization we do not explicitly 'release()' these
       * correlation id's:
       *  - There could easily be some hundreds of them, released
       *    one by one in loop.
       *  - At the innermost level the release() is more or less a NOOP
       *    as Arena allocated memory can't be released for reuse.
       */
      m_arenaAllocator.release(treeNodePtr.p->m_batchArena);
      treeNodePtr.p->m_deferred.init();

      /**
       * TreeNode-type specific cleanup.
       */
      if (treeNodePtr.p->m_info->m_parent_batch_cleanup != 0) {
        jam();
        (this->*(treeNodePtr.p->m_info->m_parent_batch_cleanup))(
            requestPtr, treeNodePtr, done);
      }
    }
  }
}

/**
 * Handle that batch for this 'TreeNode' is complete.
 */
void Dbspj::handleTreeNodeComplete(Signal *signal, Ptr<Request> requestPtr,
                                   Ptr<TreeNode> treeNodePtr) {
  if ((requestPtr.p->m_state & Request::RS_ABORTING) == 0) {
    jam();
    ndbassert(
        requestPtr.p->m_completed_tree_nodes.get(treeNodePtr.p->m_node_no));

    /**
     * If all predecessors are complete, this has to be reported
     * as we might be waiting for this condition to start more
     * operations.
     */
    if (requestPtr.p->m_completed_tree_nodes.contains(
            treeNodePtr.p->m_predecessors) &&
        !requestPtr.p->m_suspended_tree_nodes.overlaps(
            treeNodePtr.p->m_predecessors)) {
      jam();
      reportAncestorsComplete(signal, requestPtr, treeNodePtr);
    }
  }
}

/**
 * Notify any TreeNode(s) to be executed after the completed
 * TreeNode that their predecessors has completed their batch.
 */
void Dbspj::reportAncestorsComplete(Signal *signal, Ptr<Request> requestPtr,
                                    Ptr<TreeNode> treeNodePtr) {
  DEBUG("reportAncestorsComplete: " << treeNodePtr.p->m_node_no);

  if (!requestPtr.p->m_suspended_tree_nodes.get(treeNodePtr.p->m_node_no)) {
    jam();
    LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                               m_dependency_map_pool);
    Local_dependency_map nextExec(pool, treeNodePtr.p->m_next_nodes);
    Dependency_map::ConstDataBufferIterator it;

    for (nextExec.first(it); !it.isNull(); nextExec.next(it)) {
      jam();
      Ptr<TreeNode> nextTreeNodePtr;
      ndbrequire(m_treenode_pool.getPtr(nextTreeNodePtr, *it.data));

      /**
       * Notify all TreeNodes which depends on the completed predecessors.
       */
      if (requestPtr.p->m_completed_tree_nodes.contains(
              nextTreeNodePtr.p->m_predecessors)) {
        if (nextTreeNodePtr.p->m_resumeEvents & TreeNode::TN_RESUME_NODE) {
          jam();
          resumeBufferedNode(signal, requestPtr, nextTreeNodePtr);
        }

        /* Notify only TreeNodes which has requested a completion notify. */
        if (nextTreeNodePtr.p->m_bits &
            TreeNode::T_NEED_REPORT_BATCH_COMPLETED) {
          jam();
          ndbassert(nextTreeNodePtr.p->m_info != NULL);
          ndbassert(nextTreeNodePtr.p->m_info->m_parent_batch_complete != NULL);
          (this->*(nextTreeNodePtr.p->m_info->m_parent_batch_complete))(
              signal, requestPtr, nextTreeNodePtr);
        }
        reportAncestorsComplete(signal, requestPtr, nextTreeNodePtr);
      }
    }
  }
}

/**
 * Set the Request to ABORTING state, and where appropriate,
 * inform any participating LDMs about the decision to
 * terminate the query.
 *
 * NOTE: No reply is yet sent to the API. This is taken care of by
 * the outermost ::exec<FOO> methods calling either ::checkPrepareComplete()
 * or ::checkBatchComplete(), which send a CONF/REF reply when all
 * 'outstanding' work is done.
 */
void Dbspj::abort(Signal *signal, Ptr<Request> requestPtr, Uint32 errCode) {
  jam();
  if ((requestPtr.p->m_state & Request::RS_ABORTING) != 0) {
    jam();
    return;
  }

  requestPtr.p->m_state |= Request::RS_ABORTING;
  requestPtr.p->m_errCode = errCode;
  requestPtr.p->m_suspended_tree_nodes.clear();

  {
    Ptr<TreeNode> nodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    for (list.first(nodePtr); !nodePtr.isNull(); list.next(nodePtr)) {
      jam();
      ndbrequire(nodePtr.p->m_info != 0);
      if (nodePtr.p->m_info->m_abort != 0) {
        jam();
        (this->*(nodePtr.p->m_info->m_abort))(signal, requestPtr, nodePtr);
      }
    }
  }
}

Uint32 Dbspj::nodeFail(Signal *signal, Ptr<Request> requestPtr,
                       NdbNodeBitmask nodes) {
  Uint32 cnt = 0;
  Uint32 iter = 0;

  {
    Ptr<TreeNode> nodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    for (list.first(nodePtr); !nodePtr.isNull(); list.next(nodePtr)) {
      jam();
      ndbrequire(nodePtr.p->m_info != 0);
      if (nodePtr.p->m_info->m_execNODE_FAILREP != 0) {
        jam();
        iter++;
        cnt += (this->*(nodePtr.p->m_info->m_execNODE_FAILREP))(
            signal, requestPtr, nodePtr, nodes);
      }
    }
  }

  if (cnt == 0) {
    jam();
    /**
     * None of the operations needed NodeFailRep "action"
     *   check if our TC has died...but...only needed in
     *   scan case...for lookup...not so...
     */
    if (requestPtr.p->isLookup()) {
      jam();
      return 0;  // Lookup: Don't care about TC still alive
    } else if (!nodes.get(refToNode(requestPtr.p->m_senderRef))) {
      jam();
      return 0;  // Scan: Requesting TC is still alive.
    }
  }

  jam();
  abort(signal, requestPtr, DbspjErr::NodeFailure);
  checkBatchComplete(signal, requestPtr);

  return cnt + iter;
}

void Dbspj::complete(Signal *signal, Ptr<Request> requestPtr) {
  /**
   * we need to run complete-phase before sending last SCAN_FRAGCONF
   */
  Uint32 flags =
      requestPtr.p->m_state & (Request::RS_ABORTING | Request::RS_WAITING);

  requestPtr.p->m_state = Request::RS_COMPLETING | flags;

  // clear bit so that next batchComplete()
  // will continue to cleanup
  ndbassert((requestPtr.p->m_bits & Request::RT_NEED_COMPLETE) != 0);
  requestPtr.p->m_bits &= ~(Uint32)Request::RT_NEED_COMPLETE;
  ndbassert(requestPtr.p->m_outstanding == 0);
  requestPtr.p->m_outstanding = 0;
  {
    Ptr<TreeNode> nodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    for (list.first(nodePtr); !nodePtr.isNull(); list.next(nodePtr)) {
      jam();
      if (nodePtr.p->m_bits & TreeNode::T_NEED_COMPLETE) {
        jam();
        ndbassert(nodePtr.p->m_info != NULL);
        ndbassert(nodePtr.p->m_info->m_complete != NULL);
        (this->*(nodePtr.p->m_info->m_complete))(signal, requestPtr, nodePtr);
      }
    }
  }

  jam();
  checkBatchComplete(signal, requestPtr);
}

/**
 * Release as much as possible of sub objects owned by this Request,
 * including its TreeNodes.
 * The Request itself is *not* released yet as it may still be needed
 * to track the state of the request. (Set to include RS_DONE)
 */
void Dbspj::cleanup(Ptr<Request> requestPtr, bool in_hash) {
  ndbrequire(requestPtr.p->m_cnt_active == 0);
  {
    Ptr<TreeNode> nodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    while (list.removeFirst(nodePtr)) {
      jam();
      ndbrequire(nodePtr.p->m_info != 0 && nodePtr.p->m_info->m_cleanup != 0);
      (this->*(nodePtr.p->m_info->m_cleanup))(requestPtr, nodePtr);

      removeGuardedPtr(nodePtr);
      m_treenode_pool.release(nodePtr);
    }
  }
  if (requestPtr.p->isScan()) {
    jam();

    /**
     * If a Request in state RS_WAITING is aborted (node failure?),
     * there is no ongoing client request we can reply to.
     * We set it to RS_ABORTED state now, a later SCAN_NEXTREQ will
     * find the RS_ABORTED request, REF with the abort reason, and
     * then complete the cleaning up
     *
     * NOTE1: If no SCAN_NEXTREQ ever arrives for this Request, it
     *        is effectively leaked!
     *
     * NOTE2: During testing I was never able to find any SCAN_NEXTREQ
     *        arriving for a ABORTED query. So there likely are such
     *        leaks! Suspect that TC does not send SCAN_NEXTREQ to
     *        SPJ/LQH blocks affected by a node failure?
     */
    if (unlikely((requestPtr.p->m_state & Request::RS_WAITING) != 0)) {
      jam();
      ndbrequire(in_hash);
      requestPtr.p->m_state = Request::RS_ABORTED;
      return;
    }
    ndbrequire(in_hash ==
               m_scan_request_hash.remove(requestPtr, *requestPtr.p));
  } else {
    jam();
    ndbrequire(in_hash ==
               m_lookup_request_hash.remove(requestPtr, *requestPtr.p));
  }
  ArenaHead ah = requestPtr.p->m_arena;
  m_request_pool.release(requestPtr);
  m_arenaAllocator.release(ah);
}

void Dbspj::cleanup_common(Ptr<Request> requestPtr, Ptr<TreeNode> treeNodePtr) {
  jam();

  // Release TreeNode object allocated in the Request 'global' m_arena.
  // (Actually obsolete by entire Request::m_arena released later)
  LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                             m_dependency_map_pool);
  {
    Local_dependency_map list(pool, treeNodePtr.p->m_child_nodes);
    list.release();
  }

  {
    Local_pattern_store pattern(pool, treeNodePtr.p->m_keyPattern);
    pattern.release();
  }

  {
    Local_pattern_store pattern(pool, treeNodePtr.p->m_attrParamPattern);
    pattern.release();
  }

  // Correlation ids for deferred operations are allocated in the batch specific
  // arena. It is sufficient to release entire memory arena.
  m_arenaAllocator.release(treeNodePtr.p->m_batchArena);

  if (treeNodePtr.p->m_send.m_keyInfoPtrI != RNIL) {
    jam();
    releaseSection(treeNodePtr.p->m_send.m_keyInfoPtrI);
  }

  if (treeNodePtr.p->m_send.m_attrInfoPtrI != RNIL) {
    jam();
    releaseSection(treeNodePtr.p->m_send.m_attrInfoPtrI);
  }

  releasePages(treeNodePtr.p->m_rowBuffer);
}

static bool spjCheckFailFunc(const char *predicate, const char *file,
                             const unsigned line, const Uint32 instance) {
  g_eventLogger->info(
      "DBSPJ %u : Failed spjCheck (%s) "
      "at line %u of %s.",
      instance, predicate, line, file);
  return false;
}

#define spjCheck(check) \
  ((check) ? true : spjCheckFailFunc(#check, __FILE__, __LINE__, instance()))

bool Dbspj::checkRequest(const Ptr<Request> requestPtr) {
  jam();

  /**
   * We check the request, with individual assertions
   * affecting the overall result code
   * We attempt to dump the request if there's a problem
   * Dumping is done last to avoid problems with iterating
   * lists concurrently + IntrusiveList.
   * So checks should record the problem type etc, but not
   * ndbabort() immediately.  See spjCheck() above.
   */

  bool result = true;

  {
    Ptr<TreeNode> treeNodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    for (list.first(treeNodePtr); !treeNodePtr.isNull();
         list.next(treeNodePtr)) {
      jam();
      ndbrequire(treeNodePtr.p->m_info != NULL);
      if (treeNodePtr.p->m_info->m_checkNode != NULL) {
        jam();
        result &= (this->*(treeNodePtr.p->m_info->m_checkNode))(requestPtr,
                                                                treeNodePtr);
      }
    }
  }

  if (!result) {
    dumpRequest("failed checkRequest()", requestPtr);
    ndbabort();
  }

  return result;
}

/**
 * Processing of signals from LQH
 */
void Dbspj::execLQHKEYREF(Signal *signal) {
  jamEntry();

  const LqhKeyRef *ref =
      reinterpret_cast<const LqhKeyRef *>(signal->getDataPtr());

  Ptr<TreeNode> treeNodePtr;
  ndbrequire(getGuardedPtr(treeNodePtr, ref->connectPtr));

  Ptr<Request> requestPtr;
  ndbrequire(m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));
  ndbassert(
      !requestPtr.p->m_completed_tree_nodes.get(treeNodePtr.p->m_node_no));

  ndbassert(checkRequest(requestPtr));

  DEBUG("execLQHKEYREF"
        << ", node: " << treeNodePtr.p->m_node_no
        << ", request: " << requestPtr.i << ", errorCode: " << ref->errorCode);

  ndbrequire(treeNodePtr.p->m_info && treeNodePtr.p->m_info->m_execLQHKEYREF);
  (this->*(treeNodePtr.p->m_info->m_execLQHKEYREF))(signal, requestPtr,
                                                    treeNodePtr);
  jam();
  checkBatchComplete(signal, requestPtr);
}

void Dbspj::execLQHKEYCONF(Signal *signal) {
  jamEntry();

  const LqhKeyConf *conf =
      reinterpret_cast<const LqhKeyConf *>(signal->getDataPtr());
  Ptr<TreeNode> treeNodePtr;
  ndbrequire(getGuardedPtr(treeNodePtr, conf->opPtr));

  Ptr<Request> requestPtr;
  ndbrequire(m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));
  ndbassert(
      !requestPtr.p->m_completed_tree_nodes.get(treeNodePtr.p->m_node_no));

  DEBUG("execLQHKEYCONF"
        << ", node: " << treeNodePtr.p->m_node_no
        << ", request: " << requestPtr.i);

  ndbrequire(treeNodePtr.p->m_info && treeNodePtr.p->m_info->m_execLQHKEYCONF);
  (this->*(treeNodePtr.p->m_info->m_execLQHKEYCONF))(signal, requestPtr,
                                                     treeNodePtr);
  jam();
  checkBatchComplete(signal, requestPtr);
}

void Dbspj::execSCAN_FRAGREF(Signal *signal) {
  jamEntry();
  const ScanFragRef *ref =
      reinterpret_cast<const ScanFragRef *>(signal->getDataPtr());

  Ptr<ScanFragHandle> scanFragHandlePtr;
  ndbrequire(getGuardedPtr(scanFragHandlePtr, ref->senderData));
  Ptr<TreeNode> treeNodePtr;
  ndbrequire(
      m_treenode_pool.getPtr(treeNodePtr, scanFragHandlePtr.p->m_treeNodePtrI));
  Ptr<Request> requestPtr;
  ndbrequire(m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));
  ndbassert(
      !requestPtr.p->m_completed_tree_nodes.get(treeNodePtr.p->m_node_no));

  ndbassert(checkRequest(requestPtr));

  DEBUG("execSCAN_FRAGREF"
        << ", node: " << treeNodePtr.p->m_node_no
        << ", request: " << requestPtr.i << ", errorCode: " << ref->errorCode);

  Uint32 sig_len = signal->getLength();
  if (likely(sig_len == ScanFragRef::SignalLength_query)) {
    jam();
    scanFragHandlePtr.p->m_next_ref = ref->senderRef;
  }
  ndbrequire(treeNodePtr.p->m_info &&
             treeNodePtr.p->m_info->m_execSCAN_FRAGREF);
  (this->*(treeNodePtr.p->m_info->m_execSCAN_FRAGREF))(
      signal, requestPtr, treeNodePtr, scanFragHandlePtr);
  jam();
  checkBatchComplete(signal, requestPtr);
}

void Dbspj::execSCAN_HBREP(Signal *signal) {
  jamEntry();

  BlockReference senderRef = signal->senderBlockRef();
  Uint32 senderData = signal->theData[0];
  Uint32 transid1 = signal->theData[1];
  Uint32 transid2 = signal->theData[2];

  Ptr<ScanFragHandle> scanFragHandlePtr;
  ndbrequire(getGuardedPtr(scanFragHandlePtr, senderData));
  Ptr<TreeNode> treeNodePtr;
  ndbrequire(
      m_treenode_pool.getPtr(treeNodePtr, scanFragHandlePtr.p->m_treeNodePtrI));
  Ptr<Request> requestPtr;
  ndbrequire(m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));
  DEBUG("execSCAN_HBREP"
        << ", node: " << treeNodePtr.p->m_node_no
        << ", request: " << requestPtr.i);

  if (refToMain(scanFragHandlePtr.p->m_next_ref) == V_QUERY) {
    jam();
    /**
     * Since we will send signal below in send_close_scan we need to
     * save the signal data at signal reception since this is used to
     * send SCAN_HBREP to DBTC as well.
     */
    scanFragHandlePtr.p->m_next_ref = senderRef;
    if (scanFragHandlePtr.p->m_state ==
        ScanFragHandle::SFH_SCANNING_WAIT_CLOSE) {
      jam();
      send_close_scan(signal, scanFragHandlePtr, requestPtr);
    }
  }
  Uint32 ref = requestPtr.p->m_senderRef;
  signal->theData[0] = requestPtr.p->m_senderData;
  signal->theData[1] = transid1;
  signal->theData[2] = transid2;
  sendSignal(ref, GSN_SCAN_HBREP, signal, 3, JBB);
}

void Dbspj::send_close_scan(Signal *signal, Ptr<ScanFragHandle> fragPtr,
                            Ptr<Request> requestPtr) {
  fragPtr.p->m_state = ScanFragHandle::SFH_WAIT_CLOSE;
  ScanFragNextReq *req = CAST_PTR(ScanFragNextReq, signal->getDataPtrSend());
  req->requestInfo = 0;
  ScanFragNextReq::setCloseFlag(req->requestInfo, 1);
  req->transId1 = requestPtr.p->m_transId[0];
  req->transId2 = requestPtr.p->m_transId[1];
  req->batch_size_rows = 0;
  req->batch_size_bytes = 0;
  req->senderData = fragPtr.i;
  ndbrequire(refToMain(fragPtr.p->m_next_ref) != V_QUERY);
  sendSignal(fragPtr.p->m_next_ref, GSN_SCAN_NEXTREQ, signal,
             ScanFragNextReq::SignalLength, JBB);
}

void Dbspj::execSCAN_FRAGCONF(Signal *signal) {
  jamEntry();

  const ScanFragConf *conf =
      reinterpret_cast<const ScanFragConf *>(signal->getDataPtr());

#ifdef DEBUG_SCAN_FRAGREQ
  g_eventLogger->info("Dbspj::execSCAN_FRAGCONF() receiving SCAN_FRAGCONF ");
  printSCAN_FRAGCONF(stdout, signal->getDataPtrSend(), conf->total_len, DBLQH);
#endif

  Ptr<ScanFragHandle> scanFragHandlePtr;
  ndbrequire(getGuardedPtr(scanFragHandlePtr, conf->senderData));
  Ptr<TreeNode> treeNodePtr;
  ndbrequire(
      m_treenode_pool.getPtr(treeNodePtr, scanFragHandlePtr.p->m_treeNodePtrI));
  Ptr<Request> requestPtr;
  ndbrequire(m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));

  ndbassert(checkRequest(requestPtr));

  ndbassert(
      !requestPtr.p->m_completed_tree_nodes.get(treeNodePtr.p->m_node_no) ||
      requestPtr.p->m_state & Request::RS_ABORTING);

  DEBUG("execSCAN_FRAGCONF"
        << ", node: " << treeNodePtr.p->m_node_no
        << ", request: " << requestPtr.i);

  Uint32 sig_len = signal->getLength();
  if (likely(sig_len == ScanFragConf::SignalLength_query)) {
    jam();
    scanFragHandlePtr.p->m_next_ref = conf->senderRef;
  }
  ndbrequire(treeNodePtr.p->m_info &&
             treeNodePtr.p->m_info->m_execSCAN_FRAGCONF);
  (this->*(treeNodePtr.p->m_info->m_execSCAN_FRAGCONF))(
      signal, requestPtr, treeNodePtr, scanFragHandlePtr);
  jam();
  checkBatchComplete(signal, requestPtr);
}

void Dbspj::execSCAN_NEXTREQ(Signal *signal) {
  jamEntry();
  const ScanFragNextReq *req = (ScanFragNextReq *)&signal->theData[0];

#ifdef DEBUG_SCAN_FRAGREQ
  DEBUG("Incoming SCAN_NEXTREQ");
  printSCANFRAGNEXTREQ(stdout, &signal->theData[0],
                       ScanFragNextReq::SignalLength, DBLQH);
#endif

  Request key;
  key.m_transId[0] = req->transId1;
  key.m_transId[1] = req->transId2;
  key.m_senderData = req->senderData;

  Ptr<Request> requestPtr;
  if (unlikely(!m_scan_request_hash.find(requestPtr, key))) {
    jam();
    ndbrequire(ScanFragNextReq::getCloseFlag(req->requestInfo));
    return;
  }
  DEBUG("execSCAN_NEXTREQ, request: " << requestPtr.i);

#ifdef SPJ_TRACE_TIME
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const NDB_TICKS then = requestPtr.p->m_save_time;
  const Uint64 diff = NdbTick_Elapsed(then, now).microSec();
  requestPtr.p->m_sum_waiting += Uint32(diff);
  requestPtr.p->m_save_time = now;
#endif

  ndbassert(checkRequest(requestPtr));

  Uint32 state = requestPtr.p->m_state;
  requestPtr.p->m_state = state & ~Uint32(Request::RS_WAITING);

  do  // Not a loop, allows 'break' to common exit/error handling.
  {
    /**
     * A RS_ABORTED query is a 'toombstone' left behind when a
     * RS_WAITING query was aborted by node failures. The idea is
     * that the next SCAN_NEXTREQ will reply with the abort reason
     * and clean up.
     *
     * TODO: This doesn't seems to happen as assumed by design,
     *       Thus, RS_ABORTED queries are likely leaked!
     */
    if (unlikely(state == Request::RS_ABORTED)) {
      jam();
      break;
    }
    if (unlikely((state & Request::RS_ABORTING) != 0)) {
      /**
       * abort is already in progress...
       *   since RS_WAITING is cleared...it will end this request
       */
      jam();
      break;
    }
    if (ScanFragNextReq::getCloseFlag(
            req->requestInfo))  // Requested close scan
    {
      jam();
      abort(signal, requestPtr, 0);  // Stop query, no error
      break;
    }

    ndbrequire((state & Request::RS_WAITING) != 0);
    ndbrequire(requestPtr.p->m_outstanding == 0);

    /**
     * Scroll all relevant cursors...
     */
    Ptr<TreeNode> treeNodePtr;
    Local_TreeNodeCursor_list list(m_treenode_pool,
                                   requestPtr.p->m_cursor_nodes);
    Uint32 cnt_active = 0;

    for (list.first(treeNodePtr); !treeNodePtr.isNull();
         list.next(treeNodePtr)) {
      if (treeNodePtr.p->m_state == TreeNode::TN_ACTIVE) {
        jam();
        DEBUG("SCAN_NEXTREQ on TreeNode: "
              << ", m_node_no: " << treeNodePtr.p->m_node_no
              << ", w/ m_parentPtrI: " << treeNodePtr.p->m_parentPtrI);

        ndbrequire(treeNodePtr.p->m_info != 0 &&
                   treeNodePtr.p->m_info->m_execSCAN_NEXTREQ != 0);
        (this->*(treeNodePtr.p->m_info->m_execSCAN_NEXTREQ))(signal, requestPtr,
                                                             treeNodePtr);
        cnt_active++;
      } else {
        /**
         * Restart any other scans not being 'TN_ACTIVE'
         * (Only effective if 'RT_REPEAT_SCAN_RESULT')
         */
        jam();
        ndbrequire(requestPtr.p->m_bits & Request::RT_REPEAT_SCAN_RESULT);
        DEBUG("Restart TreeNode "
              << ", m_node_no: " << treeNodePtr.p->m_node_no
              << ", w/ m_parentPtrI: " << treeNodePtr.p->m_parentPtrI);

        ndbrequire(treeNodePtr.p->m_info != 0 &&
                   treeNodePtr.p->m_info->m_parent_batch_complete != 0);
        (this->*(treeNodePtr.p->m_info->m_parent_batch_complete))(
            signal, requestPtr, treeNodePtr);
      }
      if (unlikely((requestPtr.p->m_state & Request::RS_ABORTING) != 0)) {
        jam();
        break;
      }
    }  // for all treeNodes in 'm_cursor_nodes'

    /* Expected only a single ACTIVE TreeNode among the cursors */
    ndbrequire(cnt_active == 1 ||
               !(requestPtr.p->m_bits & Request::RT_REPEAT_SCAN_RESULT));
  } while (0);

  // If nothing restarted, or failed, we have to handle completion
  jam();
  checkBatchComplete(signal, requestPtr);
}

void Dbspj::execTRANSID_AI(Signal *signal) {
  jamEntry();
  TransIdAI *req = (TransIdAI *)signal->getDataPtr();
  Uint32 ptrI = req->connectPtr;

  Ptr<TreeNode> treeNodePtr;
  ndbrequire(getGuardedPtr(treeNodePtr, ptrI));
  Ptr<Request> requestPtr;
  ndbrequire(m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));

  ndbassert(checkRequest(requestPtr));
  ndbassert(
      !requestPtr.p->m_completed_tree_nodes.get(treeNodePtr.p->m_node_no));
  ndbassert(treeNodePtr.p->m_bits & TreeNode::T_EXPECT_TRANSID_AI);

  DEBUG("execTRANSID_AI"
        << ", node: " << treeNodePtr.p->m_node_no
        << ", request: " << requestPtr.i);

  /**
   * Copy the received row in SegmentedSection memory into Linear memory.
   */
  LinearSectionPtr linearPtr;
  if (signal->getNoOfSections() == 0)  // Short signal
  {
    ndbrequire(signal->getLength() >= TransIdAI::HeaderLength);
    memcpy(m_buffer1, &signal->theData[TransIdAI::HeaderLength],
           4 * (signal->getLength() - TransIdAI::HeaderLength));
    linearPtr.p = m_buffer1;
    linearPtr.sz = signal->getLength() - TransIdAI::HeaderLength;
  } else {
    SegmentedSectionPtr dataPtr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(dataPtr, 0));

    // Use the Dbspj::m_buffer1[] as temporary Linear storage for row.
    static_assert(sizeof(m_buffer1) >=
                      4 * (MAX_ATTRIBUTES_IN_TABLE + MAX_TUPLE_SIZE_IN_WORDS),
                  "Dbspj::m_buffer1[] too small");
    copy(m_buffer1, dataPtr);
    linearPtr.p = m_buffer1;
    linearPtr.sz = dataPtr.sz;
    releaseSections(handle);
  }

#if defined(DEBUG_LQHKEYREQ) || defined(DEBUG_SCAN_FRAGREQ)
  printf("execTRANSID_AI: ");
  for (Uint32 i = 0; i < linearPtr.sz; i++) printf("0x%.8x ", linearPtr.p[i]);
  printf("\n");
#endif

  /**
   * Register signal as arrived.
   */
  ndbassert(treeNodePtr.p->m_info && treeNodePtr.p->m_info->m_countSignal);
  (this->*(treeNodePtr.p->m_info->m_countSignal))(signal, requestPtr,
                                                  treeNodePtr, 1);

  /**
   * build easy-access-array for row
   */
  Uint32 tmp[2 + MAX_ATTRIBUTES_IN_TABLE];
  RowPtr::Header *header = CAST_PTR(RowPtr::Header, &tmp[0]);

  Uint32 cnt = buildRowHeader(header, linearPtr);
  ndbassert(header->m_len < NDB_ARRAY_SIZE(tmp));

  struct RowPtr row;
  row.m_matched = nullptr;
  row.m_src_node_ptrI = treeNodePtr.i;
  row.m_row_data.m_header = header;
  row.m_row_data.m_data = linearPtr.p;

  getCorrelationData(row.m_row_data, cnt - 1, row.m_src_correlation);

  do  // Dummy loop to allow 'break' into error handling
  {
    if (unlikely(requestPtr.p->m_state & Request::RS_ABORTING)) {
      jam();
      break;
    }

    // Set 'matched' bit in previous scan ancestors
    if ((requestPtr.p->m_bits & Request::RT_MULTI_SCAN) != 0) {
      RowPtr scanAncestorRow(row);
      Uint32 scanAncestorPtrI = treeNodePtr.p->m_scanAncestorPtrI;
      while (scanAncestorPtrI != RNIL)  // or 'break' below
      {
        jam();
        Ptr<TreeNode> scanAncestorPtr;
        ndbrequire(m_treenode_pool.getPtr(scanAncestorPtr, scanAncestorPtrI));
        if ((scanAncestorPtr.p->m_bits & TreeNode::T_BUFFER_MATCH) == 0) {
          jam();
          break;
        }

        getBufferedRow(scanAncestorPtr,
                       (scanAncestorRow.m_src_correlation >> 16),
                       &scanAncestorRow);

        if (scanAncestorRow.m_matched->get(treeNodePtr.p->m_node_no)) {
          jam();
          break;
        }
        scanAncestorRow.m_matched->set(treeNodePtr.p->m_node_no);
        scanAncestorPtrI = scanAncestorPtr.p->m_scanAncestorPtrI;
      }  // while
    }    // RT_MULTI_SCAN

    const bool isCongested =
        treeNodePtr.p->m_bits & TreeNode::T_CHK_CONGESTION &&
        requestPtr.p->m_outstanding >= HighlyCongestedLimit;

    /**
     * Buffer the row if either decided by execution plan, or needed by
     * congestion control logic.
     */
    if (treeNodePtr.p->m_bits & TreeNode::T_BUFFER_ANY || isCongested) {
      jam();
      DEBUG("Need to storeRow"
            << ", node: " << treeNodePtr.p->m_node_no);

      if (ERROR_INSERTED(17120) ||
          (ERROR_INSERTED(17121) && treeNodePtr.p->m_parentPtrI != RNIL) ||
          (ERROR_INSERTED(17122) &&
           refToNode(signal->getSendersBlockRef()) != getOwnNodeId())) {
        jam();
        CLEAR_ERROR_INSERT_VALUE;
        abort(signal, requestPtr, DbspjErr::OutOfRowMemory);
        break;
      }

      const Uint32 err = storeRow(treeNodePtr, row);
      if (unlikely(err != 0)) {
        jam();
        abort(signal, requestPtr, err);
        break;
      }
    }

    if (isCongested) {
      jam();
      DEBUG("Congested, store correlation-id for row"
            << ", node: " << treeNodePtr.p->m_node_no);

      /**
       * Append correlation values of congested operations being deferred
       * to a list / fifo. Upon resume, we will then be able to
       * relocate all BUFFER'ed rows for which to resume operations.
       */
      bool appended;
      {
        // Need an own scope for correlation_list, as ::lookup_abort() will
        // also construct such a list. Such nested usage is not allowed.
        LocalArenaPool<DataBufferSegment<14>> pool(treeNodePtr.p->m_batchArena,
                                                   m_dependency_map_pool);
        Local_correlation_list correlations(
            pool, treeNodePtr.p->m_deferred.m_correlations);
        appended = correlations.append(&row.m_src_correlation, 1);
      }
      if (unlikely(!appended)) {
        jam();
        abort(signal, requestPtr, DbspjErr::OutOfQueryMemory);
        break;
      }
      // Setting TreeNode as suspended will trigger later resumeCongestedNodes()
      requestPtr.p->m_suspended_tree_nodes.set(treeNodePtr.p->m_node_no);
      break;  // -> Don't startNextNodes()
    }

    // Submit operations to next-TreeNodes to be executed after 'this'
    startNextNodes(signal, requestPtr, treeNodePtr, row);
  } while (0);

  /**
   * When TreeNode is completed we might have to reply, or
   * resume other parts of the request.
   */
  if (requestPtr.p->m_completed_tree_nodes.get(treeNodePtr.p->m_node_no)) {
    jam();
    handleTreeNodeComplete(signal, requestPtr, treeNodePtr);
  }

  jam();
  checkBatchComplete(signal, requestPtr);
}

/**
 * Store the row(if T_BUFFER_ROW), or possibly only its correlationId, together
 * with:
 *  - The RowPtr::Header, describing the row
 *  - The 'Match' bitmask, if 'T_BUFFER_MATCH'
 *
 *  ----------------------------------
 *  | 'Match'-mask (1xUint32)        |  (present if T_BUFFER_MATCH)
 *  +--------------------------------+
 *  | RowPtr::Header::m_len          |  Size of Header array
 *  | RowPtr::Header::m_offset[]     |  Variable size array
 *  | ... more offsets ...           |
 *  | ... m_offfset[len-1]           |  Last offset is always 'corrId offset'
 *  +--------------------------------+
 *  | The row, or only its corrId    |  Row as described by Header
 *  | .. variable sized....          |
 *  ----------------------------------
 */
Uint32 Dbspj::storeRow(Ptr<TreeNode> treeNodePtr, const RowPtr &row) {
  RowCollection &collection = treeNodePtr.p->m_rows;
  Uint32 datalen;
  const RowPtr::Header *headptr;
  Uint32 headlen;

  Uint32 tmpHeader[2];
  if (treeNodePtr.p->m_bits &
      (TreeNode::T_BUFFER_ROW | TreeNode::T_CHK_CONGESTION)) {
    headptr = row.m_row_data.m_header;
    headlen = 1 + headptr->m_len;
    datalen = headptr->m_offset[headptr->m_len - 1] + 2;
  } else {
    // Build a header for only the 1-word correlation
    RowPtr::Header *header = CAST_PTR(RowPtr::Header, &tmpHeader[0]);
    header->m_len = 1;
    header->m_offset[0] = 0;
    headptr = header;
    headlen = 2;  // sizeof(m_len + m_offset[0])

    // 2 words: AttributeHeader + CorrelationId
    datalen = 2;
  }

  /**
   * Rows might be stored at an offset within the collection.
   * Calculate size to allocate for buffer.
   */
  const Uint32 offset = collection.rowOffset();
  const Uint32 matchlen =
      (treeNodePtr.p->m_bits & TreeNode::T_BUFFER_MATCH) ? 1 : 0;
  const Uint32 totlen = offset + matchlen + headlen + datalen;

  RowRef ref;
  Uint32 *dstptr = stackAlloc(*collection.m_base.m_rowBuffer, ref, totlen);
  if (unlikely(dstptr == NULL)) {
    jam();
    return DbspjErr::OutOfRowMemory;
  }
  Uint32 *const saved_dstptr = dstptr;
  dstptr += offset;

  // Insert 'MATCH', Header and 'ROW'/correlationId as specified
  if (matchlen > 0) {
    TreeNodeBitMask matched(treeNodePtr.p->m_dependencies);
    matched.set(treeNodePtr.p->m_node_no);
    memcpy(dstptr, &matched, sizeof(matched));
    dstptr++;
  }

  memcpy(dstptr, headptr, 4 * headlen);
  dstptr += headlen;

  if (treeNodePtr.p->m_bits &
      (TreeNode::T_BUFFER_ROW | TreeNode::T_CHK_CONGESTION)) {
    // Store entire row, include correlationId (last column)
    memcpy(dstptr, row.m_row_data.m_data, 4 * datalen);
  } else {
    // Store only the correlation-id if not 'BUFFER_ROW':
    const RowPtr::Header *header = row.m_row_data.m_header;
    const Uint32 pos = header->m_offset[header->m_len - 1];
    memcpy(dstptr, row.m_row_data.m_data + pos, 4 * datalen);
  }

  /**
   * Register row in a list or a correlationId searchable 'map'
   * Note that add_to_xxx may relocate entire memory area which
   * 'dstptr' referred, so it is not safe to use 'dstptr' *after*
   * the add_to_* below.
   */
  if (collection.m_type == RowCollection::COLLECTION_LIST) {
    NullRowRef.copyto_link(saved_dstptr);  // Null terminate list...
    add_to_list(collection.m_list, ref);
  } else {
    Uint32 error = add_to_map(collection.m_map, row.m_src_correlation, ref);
    if (unlikely(error)) return error;
  }

  return 0;
}

void Dbspj::setupRowPtr(Ptr<TreeNode> treeNodePtr, RowPtr &row,
                        const Uint32 *src) {
  ndbassert(src != NULL);
  const Uint32 offset = treeNodePtr.p->m_rows.rowOffset();
  const Uint32 matchlen =
      (treeNodePtr.p->m_bits & TreeNode::T_BUFFER_MATCH) ? 1 : 0;
  const RowPtr::Header *headptr = (RowPtr::Header *)(src + offset + matchlen);
  const Uint32 headlen = 1 + headptr->m_len;

  // Setup row, containing either entire row or only the correlationId.
  row.m_row_data.m_header = headptr;
  row.m_row_data.m_data = (Uint32 *)headptr + headlen;

  if (treeNodePtr.p->m_bits & TreeNode::T_BUFFER_MATCH) {
    row.m_matched = (TreeNodeBitMask *)(src + offset);
  } else {
    row.m_matched = NULL;
  }
}

void Dbspj::add_to_list(SLFifoRowList &list, RowRef rowref) {
  if (list.isNull()) {
    jam();
    list.m_first_row_page_id = rowref.m_page_id;
    list.m_first_row_page_pos = rowref.m_page_pos;
  } else {
    jam();
    /**
     * add last to list
     */
    RowRef last;
    last.m_page_id = list.m_last_row_page_id;
    last.m_page_pos = list.m_last_row_page_pos;
    Uint32 *const rowptr = get_row_ptr(last);
    rowref.copyto_link(rowptr);
  }

  list.m_last_row_page_id = rowref.m_page_id;
  list.m_last_row_page_pos = rowref.m_page_pos;
}

Uint32 *Dbspj::get_row_ptr(RowRef pos) {
  Ptr<RowPage> ptr;
  ndbrequire(m_page_pool.getPtr(ptr, pos.m_page_id));
  return ptr.p->m_data + pos.m_page_pos;
}

inline bool Dbspj::first(const SLFifoRowList &list,
                         SLFifoRowListIterator &iter) {
  if (list.isNull()) {
    jam();
    iter.setNull();
    return false;
  }

  iter.m_ref.m_page_id = list.m_first_row_page_id;
  iter.m_ref.m_page_pos = list.m_first_row_page_pos;
  iter.m_row_ptr = get_row_ptr(iter.m_ref);
  return true;
}

inline bool Dbspj::next(SLFifoRowListIterator &iter) {
  iter.m_ref.assign_from_link(iter.m_row_ptr);
  if (iter.m_ref.isNull()) {
    jam();
    return false;
  }
  iter.m_row_ptr = get_row_ptr(iter.m_ref);
  return true;
}

Uint32 Dbspj::add_to_map(RowMap &map, Uint32 corrVal, RowRef rowref) {
  Uint32 *mapptr;
  if (unlikely(map.isNull())) {
    jam();
    ndbassert(map.m_size > 0);
    ndbassert(map.m_rowBuffer != NULL);

    Uint32 sz16 = RowMap::MAP_SIZE_PER_REF_16 * map.m_size;
    Uint32 sz32 = (sz16 + 1) / 2;
    RowRef ref;
    mapptr = stackAlloc(*map.m_rowBuffer, ref, sz32);
    if (unlikely(mapptr == nullptr)) {
      jam();
      return DbspjErr::OutOfRowMemory;
    }
    map.assign(ref);
    map.m_elements = 0;
    map.clear(mapptr);
  } else {
    RowRef ref;
    map.copyto(ref);
    mapptr = get_row_ptr(ref);
  }

  Uint32 pos = corrVal & 0xFFFF;
  ndbrequire(pos < map.m_size);
  ndbrequire(map.m_elements < map.m_size);

  if (false) {
    /**
     * Check that *pos* is empty
     */
    RowRef check;
    map.load(mapptr, pos, check);
    ndbrequire(check.m_page_pos == 0xFFFF);
  }

  map.store(mapptr, pos, rowref);
  return 0;
}

inline bool Dbspj::first(const RowMap &map, RowMapIterator &iter) {
  if (map.isNull()) {
    jam();
    iter.setNull();
    return false;
  }

  iter.m_map_ptr = get_row_ptr(map.m_map_ref);
  iter.m_size = map.m_size;

  Uint32 pos = 0;
  while (RowMap::isNull(iter.m_map_ptr, pos) && pos < iter.m_size) pos++;

  if (pos == iter.m_size) {
    jam();
    iter.setNull();
    return false;
  }
  RowMap::load(iter.m_map_ptr, pos, iter.m_ref);
  iter.m_element_no = pos;
  iter.m_row_ptr = get_row_ptr(iter.m_ref);
  return true;
}

inline bool Dbspj::next(RowMapIterator &iter) {
  Uint32 pos = iter.m_element_no + 1;
  while (RowMap::isNull(iter.m_map_ptr, pos) && pos < iter.m_size) pos++;

  if (pos == iter.m_size) {
    jam();
    iter.setNull();
    return false;
  }
  RowMap::load(iter.m_map_ptr, pos, iter.m_ref);
  iter.m_element_no = pos;
  iter.m_row_ptr = get_row_ptr(iter.m_ref);
  return true;
}

bool Dbspj::first(const RowCollection &collection, RowIterator &iter) {
  iter.m_type = collection.m_type;
  if (iter.m_type == RowCollection::COLLECTION_LIST) {
    jam();
    return first(collection.m_list, iter.m_list);
  } else {
    jam();
    ndbassert(iter.m_type == RowCollection::COLLECTION_MAP);
    return first(collection.m_map, iter.m_map);
  }
}

bool Dbspj::next(RowIterator &iter) {
  if (iter.m_type == RowCollection::COLLECTION_LIST) {
    jam();
    return next(iter.m_list);
  } else {
    jam();
    ndbassert(iter.m_type == RowCollection::COLLECTION_MAP);
    return next(iter.m_map);
  }
}

inline Uint32 *Dbspj::stackAlloc(RowBuffer &buffer, RowRef &dst, Uint32 sz) {
  Ptr<RowPage> ptr;
  Local_RowPage_fifo list(m_page_pool, buffer.m_page_list);

  Uint32 pos = buffer.m_stack_pos;
  static const Uint32 SIZE = RowPage::SIZE;
  if (unlikely(list.isEmpty() || (pos + sz) > SIZE)) {
    jam();
    bool ret = allocPage(ptr);
    if (unlikely(ret == false)) {
      jam();
      return 0;
    }

    pos = 0;
    list.addLast(ptr);
  } else {
    list.last(ptr);
  }

  dst.m_page_id = ptr.i;
  dst.m_page_pos = pos;
  buffer.m_stack_pos = pos + sz;
  return ptr.p->m_data + pos;
}

bool Dbspj::allocPage(Ptr<RowPage> &ptr) {
  if (m_free_page_list.isEmpty()) {
    jam();
    if (ERROR_INSERTED_CLEAR(17003)) {
      jam();
      g_eventLogger->info(
          "Injecting failed '::allocPage', error 17003 at line %d file %s",
          __LINE__, __FILE__);
      return false;
    }
    ptr.p = (RowPage *)m_ctx.m_mm.alloc_page(RT_SPJ_DATABUFFER, &ptr.i,
                                             Ndbd_mem_manager::NDB_ZONE_LE_32);
    if (unlikely(ptr.p == nullptr)) {
      jam();
      return false;
    }
    m_allocedPages++;
  } else {
    jam();
    Local_RowPage_list list(m_page_pool, m_free_page_list);
    const bool ret = list.removeFirst(ptr);
    ndbrequire(ret);
  }
  const Uint32 usedPages = getUsedPages();
  if (usedPages > m_maxUsedPages) m_maxUsedPages = usedPages;
  return true;
}

/* Release all pages allocated to this RowBuffer */
void Dbspj::releasePages(RowBuffer &rowBuffer) {
  Local_RowPage_list list(m_page_pool, m_free_page_list);
  list.prependList(rowBuffer.m_page_list);
  rowBuffer.reset();
}

void Dbspj::releaseGlobal(Signal *signal) {
  // Add to statistics the max number of pages used in last 10/100ms-periode
  if (signal->theData[1] == 0) {
    m_usedPagesStat.update(m_maxUsedPages);
    m_maxUsedPages = getUsedPages();
  }

  // Get the upper 95 percentile of max pages being used
  const double used_mean = m_usedPagesStat.getMean();
  const double used_upper95 = used_mean + 2 * m_usedPagesStat.getStdDev();

  // Free excess pages if any such held in the m_free_page_list
  int cnt = 0;
  Local_RowPage_list free_list(m_page_pool, m_free_page_list);
  while (m_allocedPages > used_upper95 && !free_list.isEmpty()) {
    if (++cnt > 16)  // Take realtime break
    {
      jam();
      signal->theData[0] = 0;  // 0 -> releaseGlobal()
      signal->theData[1] = 1;  // 1 -> Continue release without new sample
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
    }
    Ptr<RowPage> ptr;
    free_list.removeFirst(ptr);
    m_ctx.m_mm.release_page(RT_SPJ_DATABUFFER, ptr.i);
    m_allocedPages--;
  }

  // If there are many free_pages, we want the sample+release to happen faster
  const Uint32 delay = free_list.getCount() > 16 ? 10 : 100;
  signal->theData[0] = 0;  // 0 -> releaseGlobal()
  signal->theData[1] = 0;  // 0 -> Take new UsedPages sample after 'delay'
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, delay, 2);
}

Uint32 Dbspj::checkTableError(Ptr<TreeNode> treeNodePtr) const {
  jam();
  if (treeNodePtr.p->m_tableOrIndexId >= c_tabrecFilesize) {
    jam();
    ndbassert(c_tabrecFilesize > 0);
    return DbspjErr::NoSuchTable;
  }

  TableRecordPtr tablePtr;
  tablePtr.i = treeNodePtr.p->m_tableOrIndexId;
  ptrAss(tablePtr, m_tableRecord);
  Uint32 err = tablePtr.p->checkTableError(treeNodePtr.p->m_schemaVersion);
  if (unlikely(err)) {
    DEBUG_DICT("Dbsp::checkTableError"
               << ", m_node_no: " << treeNodePtr.p->m_node_no
               << ", tableOrIndexId: " << treeNodePtr.p->m_tableOrIndexId
               << ", error: " << err);
  }
  if (ERROR_INSERTED(17520) || (ERROR_INSERTED(17521) && (rand() % 7) == 0)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    g_eventLogger->info(
        "::checkTableError, injecting NoSuchTable error at line %d file %s",
        __LINE__, __FILE__);
    return DbspjErr::NoSuchTable;
  }
  return err;
}

void Dbspj::dumpScanFragHandle(Ptr<ScanFragHandle> fragPtr) const {
  jam();

  g_eventLogger->info(
      "DBSPJ %u :         SFH fragid %u state %u ref 0x%x "
      "rangePtr 0x%x",
      instance(), fragPtr.p->m_fragId, fragPtr.p->m_state, fragPtr.p->m_ref,
      fragPtr.p->m_rangePtrI);
}

void Dbspj::dumpNodeCommon(const Ptr<TreeNode> treeNodePtr) const {
  jam();

  g_eventLogger->info(
      "DBSPJ %u :     TreeNode (%u) (0x%x:%p) state %u bits 0x%x "
      "tableid %u schVer 0x%x",
      instance(), treeNodePtr.p->m_node_no, treeNodePtr.i, treeNodePtr.p,
      treeNodePtr.p->m_state, treeNodePtr.p->m_bits,
      treeNodePtr.p->m_tableOrIndexId, treeNodePtr.p->m_schemaVersion);
  g_eventLogger->info(
      "DBSPJ %u :     TreeNode (%u) ptableId %u ref 0x%x "
      "correlation %u parentPtrI 0x%x",
      instance(), treeNodePtr.p->m_node_no, treeNodePtr.p->m_primaryTableId,
      treeNodePtr.p->m_send.m_ref, treeNodePtr.p->m_send.m_correlation,
      treeNodePtr.p->m_parentPtrI);
}

void Dbspj::dumpRequest(const char *reason, const Ptr<Request> requestPtr) {
  jam();

  /* TODO Add to DUMP_STATE_ORD */

  g_eventLogger->info("DBSPJ %u : Dumping request (0x%x:%p) due to %s.",
                      instance(), requestPtr.i, requestPtr.p, reason);

  g_eventLogger->info(
      "DBSPJ %u :   Request state %u bits 0x%x errCode %u "
      "senderRef 0x%x rootFragId %u",
      instance(), requestPtr.p->m_state, requestPtr.p->m_bits,
      requestPtr.p->m_errCode, requestPtr.p->m_senderRef,
      requestPtr.p->m_rootFragId);

  g_eventLogger->info(
      "DBSPJ %u :   Request transid (0x%x 0x%x) node_cnt %u "
      "active_cnt %u m_outstanding %u",
      instance(), requestPtr.p->m_transId[0], requestPtr.p->m_transId[1],
      requestPtr.p->m_node_cnt, requestPtr.p->m_cnt_active,
      requestPtr.p->m_outstanding);

  /* Iterate over request's nodes */
  {
    Ptr<TreeNode> treeNodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    for (list.first(treeNodePtr); !treeNodePtr.isNull();
         list.next(treeNodePtr)) {
      jam();
      ndbrequire(treeNodePtr.p->m_info != NULL);

      dumpNodeCommon(treeNodePtr);

      if (treeNodePtr.p->m_info->m_dumpNode != NULL) {
        jam();
        (this->*(treeNodePtr.p->m_info->m_dumpNode))(requestPtr, treeNodePtr);
      }
    }
  }

  g_eventLogger->info("DBSPJ %u : Finished dumping request (%u:%p)", instance(),
                      requestPtr.i, requestPtr.p);
}

void Dbspj::getBufferedRow(const Ptr<TreeNode> treeNodePtr, Uint32 rowId,
                           RowPtr *row) {
  DEBUG("getBufferedRow, node no: " << treeNodePtr.p->m_node_no
                                    << ", rowId: " << rowId);
  ndbassert(treeNodePtr.p->m_bits & TreeNode::T_BUFFER_ANY);

  // Set up RowPtr & RowRef for this parent row
  RowRef ref;
  ndbassert(treeNodePtr.p->m_rows.m_type == RowCollection::COLLECTION_MAP);
  treeNodePtr.p->m_rows.m_map.copyto(ref);
  const Uint32 *const mapptr = get_row_ptr(ref);

  // Relocate parent row from correlation value.
  treeNodePtr.p->m_rows.m_map.load(mapptr, rowId, ref);
  const Uint32 *const rowptr = get_row_ptr(ref);

  RowPtr _row;
  _row.m_src_node_ptrI = treeNodePtr.i;
  setupRowPtr(treeNodePtr, _row, rowptr);

  getCorrelationData(_row.m_row_data, _row.m_row_data.m_header->m_len - 1,
                     _row.m_src_correlation);
  *row = _row;
}

/**
 * resumeBufferedNode() -  Resume the execution from the specified TreeNode
 *
 * All preceding node which we depends on, has completed their
 * batches. The returned result rows from our parent node has
 * been buffered, and the match-bitmap in our scanAncestor(s)
 * are set up.
 *
 * Iterate through all our buffered parent result rows, check their
 * 'match' vs the dependencies, and submit request for the
 * qualifying rows.
 */
void Dbspj::resumeBufferedNode(Signal *signal, Ptr<Request> requestPtr,
                               Ptr<TreeNode> treeNodePtr) {
  Ptr<TreeNode> parentPtr;
  ndbrequire(m_treenode_pool.getPtr(parentPtr, treeNodePtr.p->m_parentPtrI));
  ndbassert(treeNodePtr.p->m_resumeEvents & TreeNode::TN_RESUME_NODE);
  ndbassert(parentPtr.p->m_bits & TreeNode::T_BUFFER_ROW);

  int total = 0, skipped = 0;
  RowIterator iter;
  for (first(parentPtr.p->m_rows, iter); !iter.isNull(); next(iter)) {
    RowPtr parentRow;
    jam();
    total++;

    parentRow.m_src_node_ptrI = treeNodePtr.p->m_parentPtrI;
    setupRowPtr(parentPtr, parentRow, iter.m_base.m_row_ptr);

    getCorrelationData(parentRow.m_row_data,
                       parentRow.m_row_data.m_header->m_len - 1,
                       parentRow.m_src_correlation);

    // Need to consult the Scan-ancestor(s) to determine if
    // INNER_JOIN matches were found for all of our predecessors
    Ptr<TreeNode> scanAncestorPtr(parentPtr);
    RowPtr scanAncestorRow(parentRow);
    if (treeNodePtr.p->m_parentPtrI != treeNodePtr.p->m_scanAncestorPtrI) {
      jam();
      ndbrequire(m_treenode_pool.getPtr(scanAncestorPtr,
                                        treeNodePtr.p->m_scanAncestorPtrI));
      getBufferedRow(scanAncestorPtr, (parentRow.m_src_correlation >> 16),
                     &scanAncestorRow);
    }

    while (true) {
      TreeNodeBitMask required_matches(treeNodePtr.p->m_dependencies);
      required_matches.bitAND(scanAncestorPtr.p->m_coverage);

      if (!scanAncestorRow.m_matched->contains(required_matches)) {
        DEBUG("parentRow-join SKIPPED");
        skipped++;
        break;
      }

      if (scanAncestorPtr.p->m_coverage.contains(
              treeNodePtr.p->m_dependencies)) {
        jam();
        goto row_accepted;
      }

      // Has to consult grand-ancestors to verify their matches.
      ndbrequire(m_treenode_pool.getPtr(scanAncestorPtr,
                                        scanAncestorPtr.p->m_scanAncestorPtrI));

      if ((scanAncestorPtr.p->m_bits & TreeNode::T_BUFFER_MATCH) == 0) {
        jam();
        goto row_accepted;
      }

      getBufferedRow(scanAncestorPtr, (scanAncestorRow.m_src_correlation >> 16),
                     &scanAncestorRow);
    }
    continue;  // Row skipped, didn't 'match' dependent INNER-join -> next row

  row_accepted:
    ndbassert(treeNodePtr.p->m_info != NULL);
    ndbassert(treeNodePtr.p->m_info->m_parent_row != NULL);
    (this->*(treeNodePtr.p->m_info->m_parent_row))(signal, requestPtr,
                                                   treeNodePtr, parentRow);
  }

  DEBUG("resumeBufferedNode: #buffered rows: " << total
                                               << ", skipped: " << skipped);
}

/**
 * END - MODULE GENERIC
 */

void Dbspj::resumeCongestedNode(Signal *signal, Ptr<Request> requestPtr,
                                Ptr<TreeNode> scanAncestorPtr) {
  DEBUG("resumeCongestedNode, from scanAncestor: "
        << scanAncestorPtr.p->m_node_no);

  ndbassert(scanAncestorPtr.p->isScan());
  ndbassert(scanAncestorPtr.p->m_bits & TreeNode::T_CHK_CONGESTION);
  ndbassert(
      requestPtr.p->m_suspended_tree_nodes.get(scanAncestorPtr.p->m_node_no));

  if (scanAncestorPtr.p->m_deferred.m_it.isNull()) {
    LocalArenaPool<DataBufferSegment<14>> pool(scanAncestorPtr.p->m_batchArena,
                                               m_dependency_map_pool);
    Local_correlation_list correlations(
        pool, scanAncestorPtr.p->m_deferred.m_correlations);
    ndbassert(!correlations.isEmpty());
    correlations.first(scanAncestorPtr.p->m_deferred.m_it);
    ndbassert(!scanAncestorPtr.p->m_deferred.m_it.isNull());
  }

  Uint32 resumed = 0;
  while (!scanAncestorPtr.p->m_deferred.m_it.isNull()) {
    jam();

    // Break out if congested again
    if (requestPtr.p->m_outstanding >= HighlyCongestedLimit) {
      jam();
      DEBUG("resumeCongestedNode, congested again -> break"
            << ", node: " << scanAncestorPtr.p->m_node_no << ", resumed: "
            << resumed << ", outstanding: " << requestPtr.p->m_outstanding);
      return;  // -> will re-resumeCongestedNodes() later
    }

    if (resumed > ResumeCongestedQuota) {
      jam();
      DEBUG("resumeCongestedNode, RT-break"
            << ", node: " << scanAncestorPtr.p->m_node_no << ", resumed: "
            << resumed << ", outstanding: " << requestPtr.p->m_outstanding);
      return;  // -> will re-resume when below MildlyCongestedLimit again
    }

    const Uint32 corrVal = *scanAncestorPtr.p->m_deferred.m_it.data;

    // Set up RowPtr & RowRef for this parent row
    RowPtr row;
    row.m_src_node_ptrI = scanAncestorPtr.i;
    row.m_src_correlation = corrVal;

    ndbassert(scanAncestorPtr.p->m_rows.m_type ==
              RowCollection::COLLECTION_MAP);
    RowRef ref;
    scanAncestorPtr.p->m_rows.m_map.copyto(ref);
    const Uint32 *const mapptr = get_row_ptr(ref);

    // Relocate parent row from correlation value.
    const Uint32 rowId = (corrVal & 0xFFFF);
    scanAncestorPtr.p->m_rows.m_map.load(mapptr, rowId, ref);

    const Uint32 *const rowptr = get_row_ptr(ref);
    setupRowPtr(scanAncestorPtr, row, rowptr);

    startNextNodes(signal, requestPtr, scanAncestorPtr, row);
    resumed++;
    {
      LocalArenaPool<DataBufferSegment<14>> pool(
          scanAncestorPtr.p->m_batchArena, m_dependency_map_pool);
      Local_correlation_list correlations(
          pool, scanAncestorPtr.p->m_deferred.m_correlations);
      correlations.next(scanAncestorPtr.p->m_deferred.m_it);
    }
  }
  // All deferred operations has been resumed:
  DEBUG("resumeCongestedNode: #rows: " << resumed);

  // Not congested any more, clear m_suspended state
  requestPtr.p->m_suspended_tree_nodes.clear(scanAncestorPtr.p->m_node_no);

  // Reset / release the deferred operation containers
  m_arenaAllocator.release(scanAncestorPtr.p->m_batchArena);
  scanAncestorPtr.p->m_deferred.init();

  if (requestPtr.p->m_completed_tree_nodes.get(scanAncestorPtr.p->m_node_no)) {
    jam();
    // All rows from scanAncestorPtr in this batch has been received
    handleTreeNodeComplete(signal, requestPtr, scanAncestorPtr);
  }
}  // ::resumeCongestedNodes

void Dbspj::startNextNodes(Signal *signal, Ptr<Request> requestPtr,
                           Ptr<TreeNode> treeNodePtr, const RowPtr &rowRef) {
  LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                             m_dependency_map_pool);
  Local_dependency_map nextExec(pool, treeNodePtr.p->m_next_nodes);
  Dependency_map::ConstDataBufferIterator it;

  /**
   * Activate 'next' operations to be executed, based on 'rowRef'.
   */
  for (nextExec.first(it); !it.isNull(); nextExec.next(it)) {
    Ptr<TreeNode> nextTreeNodePtr;
    ndbrequire(m_treenode_pool.getPtr(nextTreeNodePtr, *it.data));

    /**
     * Execution of 'next' TreeNode may have to be delayed as we
     * will like to see which INNER-joins which had matches first.
     * Will be resumed later by resumeBufferedNode()
     */
    if ((nextTreeNodePtr.p->m_resumeEvents & TreeNode::TN_EXEC_WAIT) == 0) {
      jam();

      /**
       * 'rowRef' is the ancestor row from the immediate ancestor in
       * the execution plan. In case this is different from the parent-treeNode
       * in the 'query', we have to find the 'real' parentRow from the
       * parent as defined in the 'query'
       */
      RowPtr parentRow(rowRef);
      if (unlikely(nextTreeNodePtr.p->m_parentPtrI != treeNodePtr.i)) {
        Ptr<TreeNode> parentPtr;
        const Uint32 parentRowId = (parentRow.m_src_correlation >> 16);
        ndbrequire(
            m_treenode_pool.getPtr(parentPtr, nextTreeNodePtr.p->m_parentPtrI));
        getBufferedRow(parentPtr, parentRowId, &parentRow);
      }

      ndbassert(nextTreeNodePtr.p->m_info != NULL);
      ndbassert(nextTreeNodePtr.p->m_info->m_parent_row != NULL);

      (this->*(nextTreeNodePtr.p->m_info->m_parent_row))(
          signal, requestPtr, nextTreeNodePtr, parentRow);

      /* Recheck RS_ABORTING as 'next' operation might have aborted */
      if (unlikely(requestPtr.p->m_state & Request::RS_ABORTING)) {
        jam();
        return;
      }
    }
  }
}

/**
 * MODULE LOOKUP
 */
const Dbspj::OpInfo Dbspj::g_LookupOpInfo = {
    &Dbspj::lookup_build,
    0,  // prepare
    &Dbspj::lookup_start,
    &Dbspj::lookup_countSignal,
    &Dbspj::lookup_execLQHKEYREF,
    &Dbspj::lookup_execLQHKEYCONF,
    0,  // execSCAN_FRAGREF
    0,  // execSCAN_FRAGCONF
    &Dbspj::lookup_parent_row,
    0,  // Dbspj::lookup_parent_batch_complete,
    0,  // Dbspj::lookup_parent_batch_repeat,
    0,  // Dbspj::lookup_parent_batch_cleanup,
    0,  // Dbspj::lookup_execSCAN_NEXTREQ
    0,  // Dbspj::lookup_complete
    &Dbspj::lookup_abort,
    &Dbspj::lookup_execNODE_FAILREP,
    &Dbspj::lookup_cleanup,
    &Dbspj::lookup_checkNode,
    &Dbspj::lookup_dumpNode};

Uint32 Dbspj::lookup_build(Build_context &ctx, Ptr<Request> requestPtr,
                           const QueryNode *qn, const QueryNodeParameters *qp) {
  Uint32 err = 0;
  Ptr<TreeNode> treeNodePtr;
  const QN_LookupNode *node = (const QN_LookupNode *)qn;
  const QN_LookupParameters *param = (const QN_LookupParameters *)qp;
  do {
    jam();
    err = DbspjErr::InvalidTreeNodeSpecification;
    if (unlikely(node->len < QN_LookupNode::NodeSize)) {
      jam();
      break;
    }

    err = DbspjErr::InvalidTreeParametersSpecification;
    DEBUG("param len: " << param->len);
    if (unlikely(param->len < QN_LookupParameters::NodeSize)) {
      jam();
      break;
    }

    err = createNode(ctx, requestPtr, treeNodePtr);
    if (unlikely(err != 0)) {
      jam();
      break;
    }

    treeNodePtr.p->m_tableOrIndexId = node->tableId;
    treeNodePtr.p->m_primaryTableId = node->tableId;
    treeNodePtr.p->m_schemaVersion = node->tableVersion;
    treeNodePtr.p->m_info = &g_LookupOpInfo;
    Uint32 transId1 = requestPtr.p->m_transId[0];
    Uint32 transId2 = requestPtr.p->m_transId[1];
    Uint32 savePointId = ctx.m_savepointId;

    Uint32 treeBits = node->requestInfo;
    Uint32 paramBits = param->requestInfo;
    // g_eventLogger->info("Dbspj::lookup_build() treeBits=%.8x paramBits=%.8x",
    //         treeBits, paramBits);
    LqhKeyReq *dst = (LqhKeyReq *)treeNodePtr.p->m_lookup_data.m_lqhKeyReq;
    {
      /**
       * static variables
       */
      dst->tcBlockref = reference();
      dst->clientConnectPtr = treeNodePtr.i;

      /**
       * TODO reference()+treeNodePtr.i is passed twice
       *   this can likely be optimized using the requestInfo-bits
       * UPDATE: This can be accomplished by *not* setApplicationAddressFlag
       *         and patch LQH to then instead use tcBlockref/clientConnectPtr
       */
      dst->transId1 = transId1;
      dst->transId2 = transId2;
      dst->savePointId = savePointId;
      dst->scanInfo = 0;
      dst->attrLen = 0;
      /** Initially set reply ref to client, do_send will set SPJ refs if
       * non-LEAF */
      dst->variableData[0] = ctx.m_resultRef;
      dst->variableData[1] = param->resultData;
      Uint32 requestInfo = 0;
      LqhKeyReq::setOperation(requestInfo, ZREAD);
      LqhKeyReq::setApplicationAddressFlag(requestInfo, 1);
      LqhKeyReq::setDirtyFlag(requestInfo, 1);
      LqhKeyReq::setSimpleFlag(requestInfo, 1);
      LqhKeyReq::setNormalProtocolFlag(requestInfo, 0);  // Assume T_LEAF
      LqhKeyReq::setCorrFactorFlag(requestInfo, 1);
      LqhKeyReq::setNoDiskFlag(requestInfo,
                               (treeBits & DABits::NI_LINKED_DISK) == 0 &&
                                   (paramBits & DABits::PI_DISK_ATTR) == 0);

      // FirstMatch in a lookup request can just be ignored
      // if (treeBits & DABits::NI_FIRST_MATCH)
      //{}
      // if (treeBits & DABits::NI_ANTI_JOIN)
      //{}

      dst->requestInfo = requestInfo;
    }

    if (treeBits & QN_LookupNode::L_UNIQUE_INDEX) {
      jam();
      treeNodePtr.p->m_bits |= TreeNode::T_UNIQUE_INDEX_LOOKUP;
    }

    Uint32 tableId = node->tableId;
    Uint32 schemaVersion = node->tableVersion;

    Uint32 tableSchemaVersion = tableId + ((schemaVersion << 16) & 0xFFFF0000);
    dst->tableSchemaVersion = tableSchemaVersion;

    ctx.m_resultData = param->resultData;
    treeNodePtr.p->m_lookup_data.m_api_resultRef = ctx.m_resultRef;
    treeNodePtr.p->m_lookup_data.m_api_resultData = param->resultData;
    treeNodePtr.p->m_lookup_data.m_outstanding = 0;

    /**
     * Parse stuff common lookup/scan-frag
     */
    struct DABuffer nodeDA, paramDA;
    nodeDA.ptr = node->optional;
    nodeDA.end = nodeDA.ptr + (node->len - QN_LookupNode::NodeSize);
    paramDA.ptr = param->optional;
    paramDA.end = paramDA.ptr + (param->len - QN_LookupParameters::NodeSize);
    err = parseDA(ctx, requestPtr, treeNodePtr, nodeDA, treeBits, paramDA,
                  paramBits);
    if (unlikely(err != 0)) {
      jam();
      break;
    }

    if (treeNodePtr.p->m_bits & TreeNode::T_ATTR_INTERPRETED) {
      jam();
      LqhKeyReq::setInterpretedFlag(dst->requestInfo, 1);
    }

    /**
     * Inherit batch size from parent
     */
    treeNodePtr.p->m_batch_size = 1;
    if (treeNodePtr.p->m_parentPtrI != RNIL) {
      jam();
      Ptr<TreeNode> parentPtr;
      ndbrequire(
          m_treenode_pool.getPtr(parentPtr, treeNodePtr.p->m_parentPtrI));
      treeNodePtr.p->m_batch_size = parentPtr.p->m_batch_size;
    }

    if (ctx.m_start_signal) {
      jam();
      Signal *signal = ctx.m_start_signal;
      const LqhKeyReq *src = (const LqhKeyReq *)signal->getDataPtr();
#ifdef NOT_YET
      Uint32 instanceNo =
          blockToInstance(signal->header.theReceiversBlockNumber);
      treeNodePtr.p->m_send.m_ref =
          numberToRef(DBLQH, instanceNo, getOwnNodeId());
#else
      treeNodePtr.p->m_send.m_ref =
          numberToRef(get_query_block_no(getOwnNodeId()),
                      getInstance(src->tableSchemaVersion & 0xFFFF,
                                  src->fragmentData & 0xFFFF),
                      getOwnNodeId());
#endif

      Uint32 hashValue = src->hashValue;
      Uint32 fragId = src->fragmentData;
      Uint32 attrLen = src->attrLen;  // fragdist-key is in here

      /**
       * assertions
       */
#ifdef VM_TRACE
      Uint32 requestInfo = src->requestInfo;
      ndbassert(LqhKeyReq::getAttrLen(attrLen) == 0);           // Only long
      ndbassert(LqhKeyReq::getScanTakeOverFlag(attrLen) == 0);  // Not supported
      ndbassert(LqhKeyReq::getReorgFlag(attrLen) ==
                ScanFragReq::REORG_ALL);  // Not supported
      ndbassert(LqhKeyReq::getOperation(requestInfo) == ZREAD);
      ndbassert(LqhKeyReq::getKeyLen(requestInfo) == 0);      // Only long
      ndbassert(LqhKeyReq::getMarkerFlag(requestInfo) == 0);  // Only read
      ndbassert(LqhKeyReq::getAIInLqhKeyReq(requestInfo) == 0);
      ndbassert(LqhKeyReq::getSeqNoReplica(requestInfo) == 0);
      ndbassert(LqhKeyReq::getLastReplicaNo(requestInfo) == 0);
      ndbassert(LqhKeyReq::getApplicationAddressFlag(requestInfo) != 0);
      ndbassert(LqhKeyReq::getSameClientAndTcFlag(requestInfo) == 0);
#endif

#ifdef TODO
      /**
       * Handle various lock-modes
       */
      static Uint8 getDirtyFlag(const UintR &requestInfo);
      static Uint8 getSimpleFlag(const UintR &requestInfo);
#endif

#ifdef VM_TRACE
      Uint32 dst_requestInfo = dst->requestInfo;
      ndbassert(LqhKeyReq::getInterpretedFlag(requestInfo) ==
                LqhKeyReq::getInterpretedFlag(dst_requestInfo));
      ndbassert(LqhKeyReq::getNoDiskFlag(requestInfo) ==
                LqhKeyReq::getNoDiskFlag(dst_requestInfo));
#endif

      dst->hashValue = hashValue;
      dst->fragmentData = fragId;
      dst->attrLen = attrLen;  // fragdist is in here

      treeNodePtr.p->m_bits |= TreeNode::T_ONE_SHOT;
    }
    return 0;
  } while (0);

  return err;
}

void Dbspj::lookup_start(Signal *signal, Ptr<Request> requestPtr,
                         Ptr<TreeNode> treeNodePtr) {
  lookup_send(signal, requestPtr, treeNodePtr);
}

void Dbspj::lookup_send(Signal *signal, Ptr<Request> requestPtr,
                        Ptr<TreeNode> treeNodePtr) {
  jam();
  if (!ERROR_INSERTED(17521))  // Avoid emulated rnd errors
  {
    // ::checkTableError() should be handled before we reach this far
    ndbassert(checkTableError(treeNodePtr) == 0);
  }

  /**
   * Count number of expected reply signals:
   *  CONF or REF reply:
   *  - Expected by every non-leaf TreeNodes
   *  - For a scan request evel leaf TreeNodes get a CONF/REF reply.
   *
   *  TRANSID_AI reply:
   *  - Expected for all TreeNodes having T_EXPECT_TRANSID_AI
   */
  Uint32 cnt = 0;

  if (requestPtr.p->isScan() || !treeNodePtr.p->isLeaf())  // CONF/REF
    cnt++;

  if (treeNodePtr.p->m_bits & TreeNode::T_EXPECT_TRANSID_AI)  // TRANSID_AI
    cnt++;

  LqhKeyReq *req = reinterpret_cast<LqhKeyReq *>(signal->getDataPtrSend());

  memcpy(req, treeNodePtr.p->m_lookup_data.m_lqhKeyReq,
         sizeof(treeNodePtr.p->m_lookup_data.m_lqhKeyReq));
  req->variableData[2] = treeNodePtr.p->m_send.m_correlation;
  req->variableData[3] = requestPtr.p->m_rootResultData;

  if (!treeNodePtr.p->isLeaf() || requestPtr.p->isScan()) {
    // Non-LEAF want reply to SPJ instead of ApiClient.
    LqhKeyReq::setNormalProtocolFlag(req->requestInfo, 1);
    req->variableData[0] = reference();
    req->variableData[1] = treeNodePtr.i;
  } else {
    jam();
    /**
     * Fake that TC sent this request,
     *   so that it can route a maybe TCKEYREF
     */
    req->tcBlockref = requestPtr.p->m_senderRef;
  }

  SectionHandle handle(this);

  Uint32 ref = treeNodePtr.p->m_send.m_ref;
  Uint32 keyInfoPtrI = treeNodePtr.p->m_send.m_keyInfoPtrI;
  Uint32 attrInfoPtrI = treeNodePtr.p->m_send.m_attrInfoPtrI;

  Uint32 err = 0;

  do {
    if (treeNodePtr.p->m_bits & TreeNode::T_ONE_SHOT) {
      jam();
      /**
       * Pass sections to send
       */
      treeNodePtr.p->m_send.m_attrInfoPtrI = RNIL;
      treeNodePtr.p->m_send.m_keyInfoPtrI = RNIL;
    } else {
      if ((treeNodePtr.p->m_bits & TreeNode::T_KEYINFO_CONSTRUCTED) == 0) {
        jam();
        Uint32 tmp = RNIL;
        if (!dupSection(tmp, keyInfoPtrI)) {
          jam();
          ndbassert(tmp == RNIL);  // Guard for memleak
          err = DbspjErr::OutOfSectionMemory;
          break;
        }

        keyInfoPtrI = tmp;
      } else {
        jam();
        treeNodePtr.p->m_send.m_keyInfoPtrI = RNIL;
      }

      if ((treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED) == 0) {
        jam();
        Uint32 tmp = RNIL;

        /**
         * Test execution terminated due to 'OutOfSectionMemory' which
         * may happen for different treeNodes in the request:
         * - 17070: Fail on any lookup_send()
         * - 17071: Fail on lookup_send() if 'isLeaf'
         * - 17072: Fail on lookup_send() if treeNode not root
         */
        if (ERROR_INSERTED(17070) ||
            (ERROR_INSERTED(17071) && treeNodePtr.p->isLeaf()) ||
            (ERROR_INSERTED(17072) && treeNodePtr.p->m_parentPtrI != RNIL)) {
          jam();
          CLEAR_ERROR_INSERT_VALUE;
          g_eventLogger->info(
              "Injecting OutOfSectionMemory error at line %d file %s", __LINE__,
              __FILE__);
          releaseSection(keyInfoPtrI);
          err = DbspjErr::OutOfSectionMemory;
          break;
        }

        if (!dupSection(tmp, attrInfoPtrI)) {
          jam();
          ndbassert(tmp == RNIL);  // Guard for memleak
          releaseSection(keyInfoPtrI);
          err = DbspjErr::OutOfSectionMemory;
          break;
        }

        attrInfoPtrI = tmp;
      } else {
        jam();
        treeNodePtr.p->m_send.m_attrInfoPtrI = RNIL;
      }
    }

    getSection(handle.m_ptr[0], keyInfoPtrI);
    getSection(handle.m_ptr[1], attrInfoPtrI);
    handle.m_cnt = 2;

    /**
     * Inject error to test LQHKEYREF handling:
     * Tampering with tableSchemaVersion such that LQH will
     * return LQHKEYREF('1227: Invalid schema version')
     * May happen for different treeNodes in the request:
     * - 17030: Fail on any lookup_send()
     * - 17031: Fail on lookup_send() if 'isLeaf'
     * - 17032: Fail on lookup_send() if treeNode not root
     */
    if (ERROR_INSERTED(17030) ||
        (ERROR_INSERTED(17031) && treeNodePtr.p->isLeaf()) ||
        (ERROR_INSERTED(17032) && treeNodePtr.p->m_parentPtrI != RNIL)) {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      req->tableSchemaVersion += (1 << 16);  // Provoke 'Invalid schema version'
    }

#if defined DEBUG_LQHKEYREQ
    g_eventLogger->info("LQHKEYREQ to %x", ref);
    printLQHKEYREQ(stdout, signal->getDataPtrSend(),
                   NDB_ARRAY_SIZE(treeNodePtr.p->m_lookup_data.m_lqhKeyReq),
                   DBLQH);
    printf("KEYINFO: ");
    print(handle.m_ptr[0], stdout);
    printf("ATTRINFO: ");
    print(handle.m_ptr[1], stdout);
#endif

    Uint32 Tnode = refToNode(ref);
    if (Tnode == getOwnNodeId()) {
      c_Counters.incr_counter(CI_LOCAL_READS_SENT, 1);
    } else {
      ndbrequire(!ERROR_INSERTED(17014));

      c_Counters.incr_counter(CI_REMOTE_READS_SENT, 1);
    }

    /**
     * Test correct abort handling if datanode not (yet)
     * connected to requesting API node.
     */
    if (ERROR_INSERTED(17530) &&
        !getNodeInfo(getResultRef(requestPtr)).m_connected) {
      jam();
      releaseSections(handle);
      err = DbspjErr::OutOfSectionMemory;  // Fake an error likely seen here
      break;
    }

    /**
     * Test execution terminated due to 'NodeFailure' which
     * may happen for different treeNodes in the request:
     * - 17020: Fail on any lookup_send()
     * - 17021: Fail on lookup_send() if 'isLeaf'
     * - 17022: Fail on lookup_send() if treeNode not root
     */
    if (ERROR_INSERTED(17020) ||
        (ERROR_INSERTED(17021) && treeNodePtr.p->isLeaf()) ||
        (ERROR_INSERTED(17022) && treeNodePtr.p->m_parentPtrI != RNIL)) {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      releaseSections(handle);
      err = DbspjErr::NodeFailure;
      break;
    }

    if (unlikely(!c_alive_nodes.get(Tnode))) {
      jam();
      releaseSections(handle);
      err = DbspjErr::NodeFailure;
      break;
    } else if (cnt > 0) {
      // Register signal 'cnt' required before completion
      jam();
      ndbassert(Tnode < NDB_ARRAY_SIZE(requestPtr.p->m_lookup_node_data));
      requestPtr.p->m_completed_tree_nodes.clear(treeNodePtr.p->m_node_no);
      requestPtr.p->m_outstanding += cnt;
      requestPtr.p->m_lookup_node_data[Tnode] += cnt;
      // number wrapped
      ndbrequire(requestPtr.p->m_lookup_node_data[Tnode] != 0);
    }
    Uint32 blockNo = refToMain(ref);
    if (blockNo == V_QUERY) {
      Uint32 instance_no = refToInstance(ref);
      if (LqhKeyReq::getNoDiskFlag(req->requestInfo)) {
        if (Tnode == getOwnNodeId() && globalData.ndbMtQueryThreads > 0) {
          jam();
          ref = get_lqhkeyreq_ref(&c_tc->m_distribution_handle, instance_no);
        }
      } else {
        /**
         * We need to put back DBLQH as receiving block number as query
         * thread for the moment cannot handle disk data requests.
         */
        ref = numberToRef(DBLQH, instance_no, Tnode);
      }
    }
    sendSignal(ref, GSN_LQHKEYREQ, signal,
               NDB_ARRAY_SIZE(treeNodePtr.p->m_lookup_data.m_lqhKeyReq), JBB,
               &handle);

    treeNodePtr.p->m_lookup_data.m_outstanding += cnt;
    if (requestPtr.p->isLookup() && treeNodePtr.p->isLeaf()) {
      jam();
      /**
       * Send TCKEYCONF with DirtyReadBit + Tnode,
       *   so that API can discover if Tnode died while waiting for result
       */
      lookup_sendLeafCONF(signal, requestPtr, treeNodePtr, Tnode);
    }
    return;
  } while (0);

  ndbrequire(err);
  jam();
  abort(signal, requestPtr, err);
}  // Dbspj::lookup_send

void Dbspj::lookup_countSignal(Signal *signal, Ptr<Request> requestPtr,
                               Ptr<TreeNode> treeNodePtr, Uint32 cnt) {
  const Uint32 Tnode = refToNode(signal->getSendersBlockRef());

  ndbassert(requestPtr.p->m_lookup_node_data[Tnode] >= cnt);
  requestPtr.p->m_lookup_node_data[Tnode] -= cnt;

  ndbassert(requestPtr.p->m_outstanding >= cnt);
  requestPtr.p->m_outstanding -= cnt;

  ndbassert(treeNodePtr.p->m_lookup_data.m_outstanding >= cnt);
  treeNodePtr.p->m_lookup_data.m_outstanding -= cnt;

  if (treeNodePtr.p->m_lookup_data.m_outstanding == 0) {
    jam();
    // We have received all rows for this treeNode in this batch.
    requestPtr.p->m_completed_tree_nodes.set(treeNodePtr.p->m_node_no);
  }

  if (!requestPtr.p->m_suspended_tree_nodes.isclear() &&
      requestPtr.p->m_outstanding <= MildlyCongestedLimit) {
    // Had congestion: Can we resume some suspended operations?
    // First try to resume from the scan ancestor of this TreeNode.
    Ptr<TreeNode> scanAncestorPtr;
    ndbrequire(m_treenode_pool.getPtr(scanAncestorPtr,
                                      treeNodePtr.p->m_scanAncestorPtrI));
    if (likely(requestPtr.p->m_suspended_tree_nodes.get(
            scanAncestorPtr.p->m_node_no))) {
      jam();
      resumeCongestedNode(signal, requestPtr, scanAncestorPtr);
    } else {
      Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
      Ptr<TreeNode> suspendedTreeNodePtr;
      list.first(suspendedTreeNodePtr);

      while (true) {
        ndbassert(!suspendedTreeNodePtr.isNull());
        if (requestPtr.p->m_suspended_tree_nodes.get(
                suspendedTreeNodePtr.p->m_node_no)) {
          jam();
          resumeCongestedNode(signal, requestPtr, suspendedTreeNodePtr);
          break;
        }
        list.next(suspendedTreeNodePtr);
      }
    }
  }
}

void Dbspj::lookup_execLQHKEYREF(Signal *signal, Ptr<Request> requestPtr,
                                 Ptr<TreeNode> treeNodePtr) {
  jam();
  const LqhKeyRef *rep = (LqhKeyRef *)signal->getDataPtr();
  const Uint32 errCode = rep->errorCode;

  c_Counters.incr_counter(CI_READS_NOT_FOUND, 1);

  DEBUG("lookup_execLQHKEYREF, errorCode:" << errCode);

  if (treeNodePtr.p->m_bits & TreeNode::T_EXPECT_TRANSID_AI) {
    // Count(==2) the REF and the non-arriving TRANSID_AI
    lookup_countSignal(signal, requestPtr, treeNodePtr, 2);
  } else {
    // Count(==1) only awaiting CONF/REF
    lookup_countSignal(signal, requestPtr, treeNodePtr, 1);
  }

  /**
   * If Request is still actively running: API need to
   * be informed about error.
   * Error code may either indicate a 'hard error' which should
   * terminate the query execution, or a 'soft error' which
   * should be signaled NDBAPI, and execution continued.
   */
  if (likely((requestPtr.p->m_state & Request::RS_ABORTING) == 0)) {
    switch (errCode) {
      case 626:  // 'Soft error' : Row not found
      case 899:  // 'Soft error' : Interpreter_exit_nok

        jam();
        /**
         * Only Lookup-request need to send TCKEYREF...
         */
        if (requestPtr.p->isLookup()) {
          jam();
          lookup_stop_branch(signal, requestPtr, treeNodePtr, errCode);
        }
        break;

      default:  // 'Hard error' : abort query
        jam();
        abort(signal, requestPtr, errCode);
        return;
    }
  }

  if (requestPtr.p->m_completed_tree_nodes.get(treeNodePtr.p->m_node_no)) {
    jam();
    // We have received all rows for this treeNode in this batch.
    handleTreeNodeComplete(signal, requestPtr, treeNodePtr);
  }
}

/**
 * lookup_stop_branch() will send required signals to the API
 * to inform that the query branch starting with 'treeNodePtr'
 * will not be executed due to 'errCode'.
 *
 * NOTE: 'errCode' is expected to be a 'soft error', like
 *       'row not found', and is *not* intended to abort
 *       entire query.
 */
void Dbspj::lookup_stop_branch(Signal *signal, Ptr<Request> requestPtr,
                               Ptr<TreeNode> treeNodePtr, Uint32 errCode) {
  ndbassert(requestPtr.p->isLookup());
  DEBUG("::lookup_stop_branch"
        << ", node: " << treeNodePtr.p->m_node_no);

  /**
   * If this is a "leaf" node, either on its own, or
   * indirectly through an unique index lookup:
   * Ordinary operation would have emitted extra TCKEYCONF
   * required for nodefail handling.
   * (In case of nodefails during final leaf REQs).
   * As API can't, or at least does not try to, tell whether
   * leaf operation is REFed by SPJ or LQH, we still have to
   * send this extra CONF as required by protocol.
   */
  if (treeNodePtr.p->isLeaf()) {
    jam();
    DEBUG("  Leaf-lookup: sending extra 'CONF' for nodefail handling");
    lookup_sendLeafCONF(signal, requestPtr, treeNodePtr, getOwnNodeId());
  }

  else if (treeNodePtr.p->m_bits & TreeNode::T_UNIQUE_INDEX_LOOKUP) {
    /**
     * UNIQUE_INDEX lookups are represented with an additional
     * child which does the lookup from UQ-index into the table
     * itself. Has to check this child for being 'leaf'.
     */
    LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                               m_dependency_map_pool);
    Local_dependency_map list(pool, treeNodePtr.p->m_child_nodes);
    Dependency_map::ConstDataBufferIterator it;
    ndbrequire(list.first(it));
    ndbrequire(list.getSize() == 1);  // should only be 1 child
    Ptr<TreeNode> childPtr;
    ndbrequire(m_treenode_pool.getPtr(childPtr, *it.data));
    if (childPtr.p->isLeaf()) {
      jam();
      DEBUG(
          "  UNUQUE_INDEX-Leaf-lookup: sending extra 'CONF' "
          "for nodefail handling");
      lookup_sendLeafCONF(signal, requestPtr, childPtr, getOwnNodeId());
    }
  }

  /**
   * Then produce the REF(errCode) which terminates this
   * tree branch.
   */
  const Uint32 resultRef = treeNodePtr.p->m_lookup_data.m_api_resultRef;
  const Uint32 resultData = treeNodePtr.p->m_lookup_data.m_api_resultData;
  TcKeyRef *ref = (TcKeyRef *)signal->getDataPtr();
  ref->connectPtr = resultData;
  ref->transId[0] = requestPtr.p->m_transId[0];
  ref->transId[1] = requestPtr.p->m_transId[1];
  ref->errorCode = errCode;
  ref->errorData = 0;

  DEBUG("  send TCKEYREF");
  sendTCKEYREF(signal, resultRef, requestPtr.p->m_senderRef);
}

/**
 * Lookup leafs in lookup requests will not receive CONF/REF
 * back to SPJ when LQH request has completed. Instead we
 * will cleanup() the request when the last leafnode KEYREQ
 * has been sent. If any of the REQuested datanodes fails
 * after this, SPJ will not detect this and be able to
 * send appropriate signals to the API to awake it from the
 * 'wait' state.
 * To get around this, we instead send an extra CONF
 * to the API which inform it about which 'node' it should
 * expect a result from. API can then discover if this
 * 'node' died while waiting for results.
 */
void Dbspj::lookup_sendLeafCONF(Signal *signal, Ptr<Request> requestPtr,
                                Ptr<TreeNode> treeNodePtr, Uint32 node) {
  ndbassert(treeNodePtr.p->isLeaf());

  const Uint32 resultRef = treeNodePtr.p->m_lookup_data.m_api_resultRef;
  const Uint32 resultData = treeNodePtr.p->m_lookup_data.m_api_resultData;
  TcKeyConf *const conf = (TcKeyConf *)signal->getDataPtr();
  conf->apiConnectPtr = RNIL;
  conf->confInfo = 0;
  conf->gci_hi = 0;
  TcKeyConf::setNoOfOperations(conf->confInfo, 1);
  conf->transId1 = requestPtr.p->m_transId[0];
  conf->transId2 = requestPtr.p->m_transId[1];
  conf->operations[0].apiOperationPtr = resultData;
  conf->operations[0].attrInfoLen = TcKeyConf::DirtyReadBit | node;
  const Uint32 sigLen = TcKeyConf::StaticLength + TcKeyConf::OperationLength;
  sendTCKEYCONF(signal, sigLen, resultRef, requestPtr.p->m_senderRef);
}

void Dbspj::lookup_execLQHKEYCONF(Signal *signal, Ptr<Request> requestPtr,
                                  Ptr<TreeNode> treeNodePtr) {
  ndbrequire(!(requestPtr.p->isLookup() && treeNodePtr.p->isLeaf()));

  if (treeNodePtr.p->m_bits & TreeNode::T_USER_PROJECTION) {
    jam();
    requestPtr.p->m_rows++;
  }

  // Count awaiting CONF. If non-leaf, there will also be a TRANSID_AI
  lookup_countSignal(signal, requestPtr, treeNodePtr, 1);

  if (requestPtr.p->m_completed_tree_nodes.get(treeNodePtr.p->m_node_no)) {
    jam();
    // We have received all rows for this treeNode in this batch.
    handleTreeNodeComplete(signal, requestPtr, treeNodePtr);
  }
}

void Dbspj::lookup_parent_row(Signal *signal, Ptr<Request> requestPtr,
                              Ptr<TreeNode> treeNodePtr, const RowPtr &rowRef) {
  jam();
  DEBUG("::lookup_parent_row"
        << ", node: " << treeNodePtr.p->m_node_no);

  /**
   * Here we need to...
   *   1) construct a key
   *   2) compute hash     (normally TC)
   *   3) get node for row (normally TC)
   */
  Uint32 err = 0;
  const Uint32 tableId = treeNodePtr.p->m_tableOrIndexId;
  const Uint32 corrVal = rowRef.m_src_correlation;

  do {
    err = checkTableError(treeNodePtr);
    if (unlikely(err != 0)) {
      jam();
      break;
    }

    /**
     * Test execution terminated due to 'OutOfQueryMemory' which
     * may happen multiple places below:
     * - 17040: Fail on any lookup_parent_row()
     * - 17041: Fail on lookup_parent_row() if 'isLeaf'
     * - 17042: Fail on lookup_parent_row() if treeNode not root
     * - 17043: Fail after last outstanding signal received.
     */
    if (ERROR_INSERTED(17040) ||
        (ERROR_INSERTED(17041) && treeNodePtr.p->isLeaf()) ||
        (ERROR_INSERTED(17042) && treeNodePtr.p->m_parentPtrI != RNIL) ||
        (ERROR_INSERTED(17043) && requestPtr.p->m_outstanding == 0)) {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      err = DbspjErr::OutOfQueryMemory;
      break;
    }

    Uint32 ptrI = RNIL;
    if (treeNodePtr.p->m_bits & TreeNode::T_KEYINFO_CONSTRUCTED) {
      jam();
      DEBUG("parent_row w/ T_KEYINFO_CONSTRUCTED");
      /**
       * Get key-pattern
       */
      LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                                 m_dependency_map_pool);
      Local_pattern_store pattern(pool, treeNodePtr.p->m_keyPattern);

      bool keyIsNull;
      err = expand(ptrI, pattern, rowRef, keyIsNull);
      if (unlikely(err != 0)) {
        jam();
        releaseSection(ptrI);
        break;
      }

      if (keyIsNull) {
        /**
         * When the key contains NULL values, an EQ-match is impossible!
         * Entire lookup request can therefore be eliminate as it is known
         * to be REFused with errorCode = 626 (Row not found).
         *
         * Scan requests can simply ignore these child LQHKEYREQs
         * as REFs are not needed by the API protocol.
         *
         * Lookup requests has to send the same KEYREFs as would have
         * been produced by LQH.
         */
        jam();
        DEBUG("Key contain NULL values: Ignore impossible KEYREQ");
        releaseSection(ptrI);
        ptrI = RNIL;

        /* Send KEYREF(errCode=626) as required by lookup request protocol */
        if (requestPtr.p->isLookup()) {
          jam();
          lookup_stop_branch(signal, requestPtr, treeNodePtr, 626);
        }

        /**
         * This possibly completed this treeNode, handle it.
         */
        if (requestPtr.p->m_completed_tree_nodes.get(
                treeNodePtr.p->m_node_no)) {
          jam();
          handleTreeNodeComplete(signal, requestPtr, treeNodePtr);
        }

        return;  // Bailout, KEYREQ would have returned KEYREF(626) anyway
      }          // keyIsNull

      ndbassert(ptrI != RNIL);
      treeNodePtr.p->m_send.m_keyInfoPtrI = ptrI;
    }  // T_KEYINFO_CONSTRUCTED

    BuildKeyReq tmp;
    err =
        computeHash(signal, tmp, tableId, treeNodePtr.p->m_send.m_keyInfoPtrI);
    if (unlikely(err != 0)) break;

    err = getNodes(signal, tmp, tableId);
    if (unlikely(err != 0)) break;

    Uint32 attrInfoPtrI = treeNodePtr.p->m_send.m_attrInfoPtrI;
    if (treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED) {
      jam();
      // Need to build a modified attrInfo, extended with a parameter
      // build with the 'attrParamPattern' applied to the parent rowRef
      DEBUG("parent_row w/ T_ATTRINFO_CONSTRUCTED");
      Uint32 tmp = RNIL;

      /**
       * Test execution terminated due to 'OutOfSectionMemory' which
       * may happen for different treeNodes in the request:
       * - 17080: Fail on lookup_parent_row
       * - 17081: Fail on lookup_parent_row: if 'isLeaf'
       * - 17082: Fail on lookup_parent_row: if treeNode not root
       */
      if (ERROR_INSERTED(17080) ||
          (ERROR_INSERTED(17081) && treeNodePtr.p->isLeaf()) ||
          (ERROR_INSERTED(17082) && treeNodePtr.p->m_parentPtrI != RNIL)) {
        jam();
        CLEAR_ERROR_INSERT_VALUE;
        g_eventLogger->info(
            "Injecting OutOfSectionMemory error at line %d file %s", __LINE__,
            __FILE__);
        err = DbspjErr::OutOfSectionMemory;
        break;
      }

      if (!dupSection(tmp, attrInfoPtrI)) {
        jam();
        ndbassert(tmp == RNIL);  // Guard for memleak
        err = DbspjErr::OutOfSectionMemory;
        break;
      }

      Uint32 org_size;
      {
        SegmentedSectionPtr ptr;
        getSection(ptr, tmp);
        org_size = ptr.sz;
      }
      Uint32 paramLen = 0;  // Set paramLen after it has been expand'ed
      if (unlikely(!appendToSection(tmp, &paramLen, 1))) {
        jam();
        releaseSection(tmp);
        err = DbspjErr::OutOfSectionMemory;
        break;
      }
      bool hasNull;
      LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                                 m_dependency_map_pool);
      Local_pattern_store pattern(pool, treeNodePtr.p->m_attrParamPattern);
      err = expand(tmp, pattern, rowRef, hasNull);
      if (unlikely(err != 0)) {
        jam();
        releaseSection(tmp);
        break;
      }

      /**
       * Set size of this parameter. Note that parameter 'hasNull' is OK.
       */
      SegmentedSectionPtr ptr;
      getSection(ptr, tmp);
      Uint32 new_size = ptr.sz;
      paramLen = new_size - org_size;
      writeToSection(tmp, org_size, &paramLen, 1);

      Uint32 *sectionptrs = ptr.p->theData;
      sectionptrs[4] = paramLen;

      // Set new constructed attrInfo, containing the constructed parameter
      treeNodePtr.p->m_send.m_attrInfoPtrI = tmp;
    }

    /**
     * Now send...
     */

    /**
     * TODO merge better with lookup_start (refactor)
     */
    {
      /* We set the upper half word of m_correlation to the tuple ID
       * of the parent, such that the API can match this tuple with its
       * parent.
       * Then we re-use the tuple ID of the parent as the
       * tuple ID for this tuple also. Since the tuple ID
       * is unique within this batch and SPJ block for the parent operation,
       * it must also be unique for this operation.
       * This ensures that lookup operations with no user projection will
       * work, since such operations will have the same tuple ID as their
       * parents. The API will then be able to match a tuple with its
       * grandparent, even if it gets no tuple for the parent operation.*/
      treeNodePtr.p->m_send.m_correlation =
          (corrVal << 16) + (corrVal & 0xffff);

      treeNodePtr.p->m_send.m_ref = tmp.receiverRef;
      LqhKeyReq *dst = (LqhKeyReq *)treeNodePtr.p->m_lookup_data.m_lqhKeyReq;
      dst->hashValue = tmp.hashInfo[0];
      dst->fragmentData = tmp.fragId;
      Uint32 attrLen = 0;
      LqhKeyReq::setDistributionKey(attrLen, tmp.fragDistKey);
      dst->attrLen = attrLen;
      lookup_send(signal, requestPtr, treeNodePtr);

      if (treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED) {
        jam();
        // restore
        treeNodePtr.p->m_send.m_attrInfoPtrI = attrInfoPtrI;
      }
    }
    return;
  } while (0);

  // If we fail it will always be a 'hard error' -> abort
  ndbrequire(err);
  jam();
  abort(signal, requestPtr, err);
}

void Dbspj::lookup_abort(Signal *signal, Ptr<Request> requestPtr,
                         Ptr<TreeNode> treeNodePtr) {
  jam();
}

Uint32 Dbspj::lookup_execNODE_FAILREP(Signal *signal, Ptr<Request> requestPtr,
                                      Ptr<TreeNode> treeNodePtr,
                                      const NdbNodeBitmask mask) {
  jam();
  Uint32 node = 0;
  Uint32 sum = 0;
  while (requestPtr.p->m_outstanding &&
         ((node = mask.find(node + 1)) != NdbNodeBitmask::NotFound)) {
    Uint32 cnt = requestPtr.p->m_lookup_node_data[node];
    sum += cnt;
    requestPtr.p->m_lookup_node_data[node] = 0;
  }

  if (sum) {
    jam();
    ndbrequire(requestPtr.p->m_outstanding >= sum);
    requestPtr.p->m_outstanding -= sum;
  }

  return sum;
}

void Dbspj::lookup_cleanup(Ptr<Request> requestPtr, Ptr<TreeNode> treeNodePtr) {
  cleanup_common(requestPtr, treeNodePtr);
}

Uint32 Dbspj::handle_special_hash(Uint32 tableId, Uint32 dstHash[4],
                                  const Uint32 *src,
                                  Uint32 srcLen,  // Len in #32bit words
                                  const KeyDescriptor *desc) {
  Uint32 workspace[MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY];
  const bool hasVarKeys = desc->noOfVarKeys > 0;
  const bool hasCharAttr = desc->hasCharAttr;
  const bool compute_distkey = desc->noOfDistrKeys > 0;

  const Uint32 *hashInput = NULL;
  Uint32 inputLen = 0;
  Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX];
  Uint32 *keyPartLenPtr;

  /* Normalise KeyInfo into workspace if necessary */
  if (hasCharAttr || (compute_distkey && hasVarKeys)) {
    hashInput = workspace;
    keyPartLenPtr = keyPartLen;
    inputLen = xfrm_key_hash(tableId, src, workspace, sizeof(workspace) >> 2,
                             keyPartLenPtr);
    if (unlikely(inputLen == 0)) {
      return 290;  // 'Corrupt key in TC, unable to xfrm'
    }
  } else {
    /* Keyinfo already suitable for hash */
    hashInput = src;
    inputLen = srcLen;
    keyPartLenPtr = 0;
  }

  /* Calculate primary key hash */
  md5_hash(dstHash, hashInput, inputLen);

  /* If the distribution key != primary key then we have to
   * form a distribution key from the primary key and calculate
   * a separate distribution hash based on this
   */
  if (compute_distkey) {
    jam();

    Uint32 distrKeyHash[4];
    /* Reshuffle primary key columns to get just distribution key */
    Uint32 len = create_distr_key(tableId, hashInput, workspace, keyPartLenPtr);
    /* Calculate distribution key hash */
    md5_hash(distrKeyHash, workspace, len);

    /* Just one word used for distribution */
    dstHash[1] = distrKeyHash[1];
  }
  return 0;
}

Uint32 Dbspj::computeHash(Signal *signal, BuildKeyReq &dst, Uint32 tableId,
                          Uint32 ptrI) {
  /**
   * Essentially the same code as in Dbtc::hash().
   * The code for user defined partitioning has been removed though.
   */
  SegmentedSectionPtr ptr;
  getSection(ptr, ptrI);

  Uint32 tmp32[MAX_KEY_SIZE_IN_WORDS];
  ndbassert(ptr.sz <= MAX_KEY_SIZE_IN_WORDS);
  copy(tmp32, ptr);

  const KeyDescriptor *desc = g_key_descriptor_pool.getPtr(tableId);
  ndbrequire(desc != NULL);

  bool need_special_hash = desc->hasCharAttr | (desc->noOfDistrKeys > 0);
  if (need_special_hash) {
    jam();
    return handle_special_hash(tableId, dst.hashInfo, tmp32, ptr.sz, desc);
  } else {
    jam();
    md5_hash(dst.hashInfo, tmp32, ptr.sz);
    return 0;
  }
}

/**
 * This function differs from computeHash in that *ptrI*
 * only contains partition key (packed) and not full primary key
 */
Uint32 Dbspj::computePartitionHash(Signal *signal, BuildKeyReq &dst,
                                   Uint32 tableId, Uint32 ptrI) {
  SegmentedSectionPtr ptr;
  getSection(ptr, ptrI);

  Uint32 _space[MAX_KEY_SIZE_IN_WORDS];
  Uint32 *tmp32 = _space;
  Uint32 sz = ptr.sz;
  ndbassert(ptr.sz <= MAX_KEY_SIZE_IN_WORDS);
  copy(tmp32, ptr);

  const KeyDescriptor *desc = g_key_descriptor_pool.getPtr(tableId);
  ndbrequire(desc != NULL);

  bool need_xfrm = desc->hasCharAttr || desc->noOfVarKeys;
  if (need_xfrm) {
    jam();
    /**
     * xfrm distribution key
     */
    Uint32 srcPos = 0;
    Uint32 dstPos = 0;
    Uint32 *src = tmp32;
    Uint32 *dst = signal->theData + 24;
    for (Uint32 i = 0; i < desc->noOfKeyAttr; i++) {
      const KeyDescriptor::KeyAttr &keyAttr = desc->keyAttr[i];
      if (AttributeDescriptor::getDKey(keyAttr.attributeDescriptor)) {
        Uint32 attrLen = xfrm_attr_hash(
            keyAttr.attributeDescriptor, keyAttr.charsetInfo, src, srcPos, dst,
            dstPos, NDB_ARRAY_SIZE(signal->theData) - 24);
        if (unlikely(attrLen == 0)) {
          DEBUG_CRASH();
          return 290;  // 'Corrupt key in TC, unable to xfrm'
        }
      }
    }
    tmp32 = dst;
    sz = dstPos;
  }

  md5_hash(dst.hashInfo, tmp32, sz);
  return 0;
}

/**
 * This method comes in with a list of nodes.
 * We have already verified that our own node
 * isn't in this list. If we have a node in this
 * list that is in the same location domain as
 * this node, it will be selected before any
 * other node. So we will always try to keep
 * the read coming from the same location domain.
 *
 * To avoid radical imbalances we provide a bit
 * of round robin on a node bases. It isn't
 * any perfect round robin. We simply rotate a
 * bit among the selected nodes instead of
 * always selecting the first one we find.
 */
Uint32 Dbspj::check_own_location_domain(const Uint32 *nodes, Uint32 end) {
  Uint32 loc_nodes[MAX_NDB_NODES];
  Uint32 loc_node_count = 0;
  Uint32 my_location_domain_id = m_location_domain_id[getOwnNodeId()];

  if (my_location_domain_id == 0) {
    jam();
    return 0;
  }
  for (Uint32 i = 0; i < end; i++) {
    jam();
    Uint32 node = nodes[i];
    ndbrequire(node != 0 && node < MAX_NDB_NODES);
    if (my_location_domain_id == m_location_domain_id[node]) {
      jam();
      loc_nodes[loc_node_count++] = node;
    }
  }
  if (loc_node_count != 0) {
    jam();
    /**
     * If many nodes in the same location domain we will
     * spread the load on them by using a very simple load
     * balancing routine.
     */
    m_load_balancer_location++;
    Uint32 ret_node = loc_nodes[m_load_balancer_location % loc_node_count];
    return ret_node;
  }
  return 0;
}

Uint32 Dbspj::getNodes(Signal *signal, BuildKeyReq &dst, Uint32 tableId) {
  TableRecordPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, c_tabrecFilesize, m_tableRecord);

  DiGetNodesReq *req = (DiGetNodesReq *)&signal->theData[0];
  req->tableId = tableId;
  req->hashValue = dst.hashInfo[1];
  req->distr_key_indicator = 0;  // userDefinedPartitioning not supported!
  req->scan_indicator = 0;
  req->anyNode = (tablePtr.p->m_flags & TableRecord::TR_FULLY_REPLICATED) != 0;
  req->get_next_fragid_indicator = 0;
  req->jamBufferPtr = jamBuffer();

  EXECUTE_DIRECT_MT(DBDIH, GSN_DIGETNODESREQ, signal,
                    DiGetNodesReq::SignalLength, 0);
  jamEntry();

  DiGetNodesConf *conf = (DiGetNodesConf *)&signal->theData[0];
  const Uint32 err = signal->theData[0] ? signal->theData[1] : 0;
  Uint32 Tdata2 = conf->reqinfo;
  Uint32 nodeId = conf->nodes[0];
  Uint32 instanceKey = conf->instanceKey;

  DEBUG("HASH to nodeId:" << nodeId << ", instanceKey:" << instanceKey);

  jamEntry();
  if (unlikely(err != 0)) {
    jam();
    goto error;
  }

  /**
   * SPJ only does committed-read (for now)
   *   so it's always ok to READ_BACKUP
   *   if applicable
   */
  if (nodeId != getOwnNodeId() &&
      tablePtr.p->m_flags & TableRecord::TR_READ_BACKUP) {
    /* Node cnt from DIH ignores primary, presumably to fit in 2 bits */
    Uint32 cnt = (Tdata2 & 3) + 1;
    for (Uint32 i = 1; i < cnt; i++) {
      jam();
      if (conf->nodes[i] == getOwnNodeId()) {
        jam();
        nodeId = getOwnNodeId();
        break;
      }
    }
    if (nodeId != getOwnNodeId()) {
      Uint32 node;
      jam();
      if ((node = check_own_location_domain(&conf->nodes[0], cnt)) != 0) {
        nodeId = node;
      }
    }
  }

  dst.fragId = conf->fragId;
  dst.fragDistKey = (Tdata2 >> 16) & 255;
  dst.receiverRef = numberToRef(get_query_block_no(nodeId),
                                getInstanceNo(nodeId, instanceKey), nodeId);

  return 0;

error:
  return err;
}

bool Dbspj::lookup_checkNode(const Ptr<Request> requestPtr,
                             const Ptr<TreeNode> treeNodePtr) {
  jam();

  /* TODO */

  return true;
}

void Dbspj::lookup_dumpNode(const Ptr<Request> requestPtr,
                            const Ptr<TreeNode> treeNodePtr) {
  jam();

  const LookupData &data = treeNodePtr.p->m_lookup_data;

  g_eventLogger->info(
      "DBSPJ %u :       LOOKUP api_resultRef 0x%x "
      "resultData %u outstanding %u",
      instance(), data.m_api_resultRef, data.m_api_resultData,
      data.m_outstanding);

  /* TODO : Dump LQHKEYREQ */
}

/**
 * END - MODULE LOOKUP
 */

/**
 * MODULE SCAN FRAGMENT
 *
 * NOTE: This may not be root-node
 */
const Dbspj::OpInfo Dbspj::g_ScanFragOpInfo = {
    &Dbspj::scanFrag_build,
    &Dbspj::scanFrag_prepare,
    &Dbspj::scanFrag_start,
    &Dbspj::scanFrag_countSignal,
    0,  // execLQHKEYREF
    0,  // execLQHKEYCONF
    &Dbspj::scanFrag_execSCAN_FRAGREF,
    &Dbspj::scanFrag_execSCAN_FRAGCONF,
    &Dbspj::scanFrag_parent_row,
    &Dbspj::scanFrag_parent_batch_complete,
    &Dbspj::scanFrag_parent_batch_repeat,
    &Dbspj::scanFrag_parent_batch_cleanup,
    &Dbspj::scanFrag_execSCAN_NEXTREQ,
    &Dbspj::scanFrag_complete,
    &Dbspj::scanFrag_abort,
    &Dbspj::scanFrag_execNODE_FAILREP,
    &Dbspj::scanFrag_cleanup,
    &Dbspj::scanFrag_checkNode,
    &Dbspj::scanFrag_dumpNode};

Uint32 Dbspj::scanFrag_build(Build_context &ctx, Ptr<Request> requestPtr,
                             const QueryNode *qn,
                             const QueryNodeParameters *qp) {
  Uint32 err = 0;
  Ptr<TreeNode> treeNodePtr;
  const QN_ScanFragNode *node = (const QN_ScanFragNode *)qn;
  const QN_ScanFragParameters *param = (const QN_ScanFragParameters *)qp;

  // Only scan requests can have scan-TreeNodes
  ndbassert(requestPtr.p->isScan());

  do {
    jam();
    err = DbspjErr::InvalidTreeNodeSpecification;
    DEBUG("scanFrag_build: len=" << node->len);
    if (unlikely(node->len < QN_ScanFragNode::NodeSize)) {
      jam();
      break;
    }

    err = DbspjErr::InvalidTreeParametersSpecification;
    DEBUG("param len: " << param->len);
    if (unlikely(param->len < QN_ScanFragParameters::NodeSize)) {
      jam();
      break;
    }

    err = createNode(ctx, requestPtr, treeNodePtr);
    if (unlikely(err != 0)) {
      jam();
      break;
    }

    const Uint32 treeBits = node->requestInfo;
    const Uint32 paramBits = param->requestInfo;
    const Uint32 batchRows = param->batch_size_rows;
    const Uint32 batchBytes = param->batch_size_bytes;
    const Uint32 indexId = node->tableId;
    const Uint32 tableId =
        g_key_descriptor_pool.getPtr(indexId)->primaryTableId;

    treeNodePtr.p->m_info = &g_ScanFragOpInfo;
    treeNodePtr.p->m_tableOrIndexId = indexId;
    treeNodePtr.p->m_primaryTableId = tableId;
    treeNodePtr.p->m_schemaVersion = node->tableVersion;
    treeNodePtr.p->m_bits |= TreeNode::T_ATTR_INTERPRETED;
    treeNodePtr.p->m_batch_size = batchRows;

    ctx.m_resultData = param->resultData;

    /**
     * Parse stuff
     */
    struct DABuffer nodeDA, paramDA;
    nodeDA.ptr = node->optional;
    nodeDA.end = nodeDA.ptr + (node->len - QN_ScanFragNode::NodeSize);
    paramDA.ptr = param->optional;
    paramDA.end = paramDA.ptr + (param->len - QN_ScanFragParameters::NodeSize);

    err = parseScanFrag(ctx, requestPtr, treeNodePtr, nodeDA, treeBits, paramDA,
                        paramBits);

    if (unlikely(err != 0)) {
      jam();
      break;
    }

    /**
     * If there exists other scan TreeNodes not being among
     * my ancestors, results from this scanFrag may be repeated
     * as part of an X-scan.
     *
     * NOTE: The scan nodes being along the left deep ancestor chain
     *       are not 'repeatable' as they are driving the
     *       repeated X-scan and are thus not repeated themself.
     */
    if (requestPtr.p->m_bits & Request::RT_REPEAT_SCAN_RESULT &&
        !treeNodePtr.p->m_ancestors.contains(ctx.m_scans)) {
      treeNodePtr.p->m_bits |= TreeNode::T_SCAN_REPEATABLE;
    }

    ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
    ScanFragReq *const dst =
        reinterpret_cast<ScanFragReq *>(data.m_scanFragReq);

    /**
     * The root node get most of its ScanFragReq contents readily
     * filled in from the 'start_signal'. So building the initial
     * contents of the m_scanFragReq has to be handled different
     * for the root node vs. a non-root node.
     */
    if (ctx.m_start_signal)  // Is the root node?
    {
      jam();
      ndbassert(treeNodePtr.p->m_parentPtrI == RNIL);

      /**
       * The REQuest in 'start_signal' contains most of the m_scanFragReq
       * readilly filled in. Copy it, and modify where needed.
       */
      const Signal *signal = ctx.m_start_signal;
      const ScanFragReq *const req =
          reinterpret_cast<const ScanFragReq *>(signal->getDataPtr());
      memcpy(dst, req, sizeof(data.m_scanFragReq));

      // Assert some limitations on the SPJ supported ScanFragReq
      ndbassert(ScanFragReq::getLockMode(req->requestInfo) == 0);
      ndbassert(ScanFragReq::getHoldLockFlag(req->requestInfo) == 0);
      ndbassert(ScanFragReq::getKeyinfoFlag(req->requestInfo) == 0);
      ndbassert(ScanFragReq::getReadCommittedFlag(req->requestInfo) == 1);
      ndbassert(ScanFragReq::getLcpScanFlag(req->requestInfo) == 0);
      ndbassert(ScanFragReq::getReorgFlag(req->requestInfo) ==
                ScanFragReq::REORG_ALL);

      /**
       * 'NoDiskFlag' should agree with information in treeNode
       */
      ndbassert(ScanFragReq::getNoDiskFlag(req->requestInfo) ==
                ((treeBits & DABits::NI_LINKED_DISK) == 0 &&
                 (paramBits & DABits::PI_DISK_ATTR) == 0));

      ndbassert(dst->savePointId == ctx.m_savepointId);
      ndbassert(dst->tableId == node->tableId);
      ndbassert(dst->schemaVersion == node->tableVersion);
      ndbassert(dst->transId1 == requestPtr.p->m_transId[0]);
      ndbassert(dst->transId2 == requestPtr.p->m_transId[1]);

      treeNodePtr.p->m_bits |= TreeNode::T_ONE_SHOT;

      TableRecordPtr tablePtr;
      tablePtr.i = treeNodePtr.p->m_tableOrIndexId;
      ptrCheckGuard(tablePtr, c_tabrecFilesize, m_tableRecord);
      const bool readBackup =
          (tablePtr.p->m_flags & TableRecord::TR_READ_BACKUP) != 0;

      data.m_fragCount = 0;

      /**
       * As this is the root node, fragId is already contained in the REQuest.
       * Fill in the set of 'm_fragments' to be SCAN'ed by this REQ.
       */
      {
        Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);

        if (ScanFragReq::getMultiFragFlag(req->requestInfo)) {
          jam();
          Uint32 variableLen = 25;
          data.m_fragCount = signal->theData[variableLen++];
          for (Uint32 i = 0; i < data.m_fragCount; i++) {
            jam();
            Ptr<ScanFragHandle> fragPtr;
            const Uint32 fragId = signal->theData[variableLen++];
            const Uint32 ref =
                numberToRef(get_query_block_no(getOwnNodeId()),
                            getInstance(req->tableId, fragId), getOwnNodeId());

            DEBUG("Scan build, fragId: " << fragId << ", ref: " << ref);

            if (!ERROR_INSERTED_CLEAR(17004) &&
                likely(m_scanfraghandle_pool.seize(requestPtr.p->m_arena,
                                                   fragPtr))) {
              fragPtr.p->init(fragId, readBackup);
              fragPtr.p->m_treeNodePtrI = treeNodePtr.i;
              fragPtr.p->m_ref = ref;
              fragPtr.p->m_next_ref = ref;
              list.addLast(fragPtr);
              insertGuardedPtr(requestPtr, fragPtr);
            } else {
              jam();
              err = DbspjErr::OutOfQueryMemory;
              return err;
            }
          }
        } else  // 'not getMultiFragFlag(req->requestInfo)'
        {
          jam();
          Ptr<ScanFragHandle> fragPtr;
          data.m_fragCount = 1;

          const Uint32 ref = numberToRef(
              get_query_block_no(getOwnNodeId()),
              getInstance(req->tableId, req->fragmentNoKeyLen), getOwnNodeId());

          if (!ERROR_INSERTED_CLEAR(17004) &&
              likely(m_scanfraghandle_pool.seize(requestPtr.p->m_arena,
                                                 fragPtr))) {
            jam();
            fragPtr.p->init(req->fragmentNoKeyLen, readBackup);
            fragPtr.p->m_treeNodePtrI = treeNodePtr.i;
            fragPtr.p->m_ref = ref;
            fragPtr.p->m_next_ref = ref;
            list.addLast(fragPtr);
            insertGuardedPtr(requestPtr, fragPtr);
          } else {
            jam();
            err = DbspjErr::OutOfQueryMemory;
            return err;
          }
        }
        requestPtr.p->m_rootFragCnt = data.m_fragCount;
      }

      if (ScanFragReq::getRangeScanFlag(req->requestInfo)) {
        c_Counters.incr_counter(CI_RANGE_SCANS_RECEIVED, 1);
      } else {
        c_Counters.incr_counter(CI_TABLE_SCANS_RECEIVED, 1);
      }
    } else {
      requestPtr.p->m_bits |= Request::RT_NEED_PREPARE;
      requestPtr.p->m_bits |= Request::RT_NEED_COMPLETE;

      treeNodePtr.p->m_bits |= TreeNode::T_NEED_PREPARE;
      treeNodePtr.p->m_bits |= TreeNode::T_NEED_COMPLETE;
      treeNodePtr.p->m_bits |= TreeNode::T_NEED_REPORT_BATCH_COMPLETED;

      dst->tableId = node->tableId;
      dst->schemaVersion = node->tableVersion;
      dst->fragmentNoKeyLen = 0xff;  // Filled in after 'prepare'
      dst->savePointId = ctx.m_savepointId;
      dst->transId1 = requestPtr.p->m_transId[0];
      dst->transId2 = requestPtr.p->m_transId[1];

      Uint32 requestInfo = 0;
      ScanFragReq::setReadCommittedFlag(requestInfo, 1);
      ScanFragReq::setScanPrio(requestInfo, ctx.m_scanPrio);
      ScanFragReq::setRangeScanFlag(requestInfo, 1);
      ScanFragReq::setNoDiskFlag(requestInfo,
                                 (treeBits & DABits::NI_LINKED_DISK) == 0 &&
                                     (paramBits & DABits::PI_DISK_ATTR) == 0);

      if (treeBits & DABits::NI_FIRST_MATCH && treeNodePtr.p->isLeaf()) {
        // Can only push firstMatch elimination to data nodes if results does
        // not depends of finding matches from children -> has to be a leaf
        ScanFragReq::setFirstMatchFlag(requestInfo, 1);
      }
      if (treeBits & DABits::NI_ANTI_JOIN && treeNodePtr.p->isLeaf()) {
        // ANTI_JOIN's cares about whether a match was found or not
        // Thus, returning only the first match is sufficient here as well
        ScanFragReq::setFirstMatchFlag(requestInfo, 1);
      }
      dst->requestInfo = requestInfo;
    }

    // Common part whether root or not
    dst->senderData = treeNodePtr.i;
    dst->resultRef = reference();
    dst->resultData = treeNodePtr.i;
    ScanFragReq::setCorrFactorFlag(dst->requestInfo, 1);
    ScanFragReq::setMultiFragFlag(dst->requestInfo, 0);

    dst->batch_size_rows = batchRows;
    dst->batch_size_bytes = batchBytes;

    ctx.m_scan_cnt++;
    ctx.m_scans.set(treeNodePtr.p->m_node_no);

    return 0;
  } while (0);

  return err;
}

Uint32 Dbspj::parseScanFrag(Build_context &ctx, Ptr<Request> requestPtr,
                            Ptr<TreeNode> treeNodePtr, DABuffer tree,
                            Uint32 treeBits, DABuffer param, Uint32 paramBits) {
  Uint32 err = 0;

  typedef QN_ScanFragNode Node;
  typedef QN_ScanFragParameters Params;

  do {
    jam();

    new (&treeNodePtr.p->m_scanFrag_data) ScanFragData;
    ScanFragData &data = treeNodePtr.p->m_scanFrag_data;

    /**
     * We will need to look at the parameters again if the scan is pruned and
     * the prune key uses parameter values. Therefore, we keep a reference to
     * the start of the parameter buffer.
     */
    DABuffer origParam = param;
    err =
        parseDA(ctx, requestPtr, treeNodePtr, tree, treeBits, param, paramBits);
    if (unlikely(err != 0)) break;

    if (treeBits & Node::SF_PRUNE_PATTERN) {
      Uint32 len_cnt = *tree.ptr++;
      Uint32 len = len_cnt & 0xFFFF;  // length of pattern in words
      Uint32 cnt = len_cnt >> 16;     // no of parameters

      LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                                 m_dependency_map_pool);
      ndbrequire((cnt == 0) == ((treeBits & Node::SF_PRUNE_PARAMS) == 0));
      ndbrequire((cnt == 0) == ((paramBits & Params::SFP_PRUNE_PARAMS) == 0));

      if (treeBits & Node::SF_PRUNE_LINKED) {
        jam();
        DEBUG("LINKED-PRUNE PATTERN w/ " << cnt << " PARAM values");

        data.m_prunePattern.init();
        Local_pattern_store pattern(pool, data.m_prunePattern);

        /**
         * Expand pattern into a new pattern (with linked values)
         */
        err = expand(pattern, treeNodePtr, tree, len, origParam, cnt);
        if (unlikely(err != 0)) {
          jam();
          break;
        }
        treeNodePtr.p->m_bits |= TreeNode::T_PRUNE_PATTERN;
        c_Counters.incr_counter(CI_PRUNED_RANGE_SCANS_RECEIVED, 1);
      } else {
        jam();
        DEBUG("FIXED-PRUNE w/ " << cnt << " PARAM values");

        /**
         * Expand pattern directly into
         *   This means a "fixed" pruning from here on
         *   i.e guaranteed single partition
         */
        Uint32 prunePtrI = RNIL;
        bool hasNull;
        err = expand(prunePtrI, tree, len, origParam, cnt, hasNull);
        if (unlikely(err != 0)) {
          jam();
          releaseSection(prunePtrI);
          break;
        }

        if (unlikely(hasNull)) {
          /* API should have eliminated requests w/ const-NULL keys */
          jam();
          DEBUG("BEWARE: T_CONST_PRUNE-key contain NULL values");
          releaseSection(prunePtrI);
          //        treeNodePtr.p->m_bits |= TreeNode::T_NULL_PRUNE;
          //        break;
          ndbabort();
        }
        ndbrequire(prunePtrI != RNIL); /* todo: can we allow / take advantage of
                                          NULLs in range scan? */
        data.m_constPrunePtrI = prunePtrI;

        /**
         * We may not compute the partition for the hash-key here
         *   as we have not yet opened a read-view
         */
        treeNodePtr.p->m_bits |= TreeNode::T_CONST_PRUNE;
        c_Counters.incr_counter(CI_CONST_PRUNED_RANGE_SCANS_RECEIVED, 1);
      }
    }  // SF_PRUNE_PATTERN

    if ((treeNodePtr.p->m_bits & TreeNode::T_CONST_PRUNE) == 0 &&
        ((treeBits & Node::SF_PARALLEL) ||
         (paramBits & Params::SFP_PARALLEL))) {
      jam();
      treeNodePtr.p->m_bits |= TreeNode::T_SCAN_PARALLEL;
    }

    /**
     * SPJ implementation of sorted result is quirky, and comes with some
     * performance impact.
     *
     * Even if we can specify sorted result order for an index scan, we might
     * need multiple batch rounds to retrieve results from the scan children
     * of the ordered index scan ('MULTI_SCAN'). As the ancestor rows for such
     * MULTI_SCANs are repeated together with the newly retrieved child-rows,
     * the ordered rows will become unordered. Note that there are no such
     * problems if all children of the ordered index scan are single row lookup
     * operations, thus the 'MULTI-SCAN' flag.
     *
     * We work around this limitation by retrieving only a single row at a time
     * from the ordered index scan:
     *  - T_SORTED_ORDER is set on the ordered index scan treeNode(below)
     *  - When sending a SCAN_FRAGREQ or SCAN_NEXTREQ, 'batch_size_rows=1'
     *    is set in the REQuest if 'T_SORTED_ORDER && MULTI_SCAN'.
     *  - As an optimization we might send more SCAN_NEXTREQs when the
     *    previous REQ has completed, if there is free batch buffer space.
     *  Note:
     *  - 'Only a single row at a time' also implies that we fetch from
     *     a single fragment only.
     *  - We support T_SORTED_ORDER only for the root-treeNode
     */
    if (paramBits & Params::SFP_SORTED_ORDER) {
      jam();
      treeNodePtr.p->m_bits |= TreeNode::T_SORTED_ORDER;
    }

    return 0;
  } while (0);

  jam();
  return err;
}

void Dbspj::scanFrag_prepare(Signal *signal, Ptr<Request> requestPtr,
                             Ptr<TreeNode> treeNodePtr) {
  jam();

  if (!ERROR_INSERTED(17521))  // Avoid emulated rnd errors
  {
    // ::checkTableError() should be handled before we reach this far
    ndbassert(checkTableError(treeNodePtr) == 0);  // Handled in Dbspj::start
  }
  ndbassert(treeNodePtr.p->m_state == TreeNode::TN_BUILDING);
  treeNodePtr.p->m_state = TreeNode::TN_PREPARING;

  requestPtr.p->m_outstanding++;

  DihScanTabReq *req = (DihScanTabReq *)signal->getDataPtrSend();
  req->tableId = treeNodePtr.p->m_tableOrIndexId;
  req->schemaTransId = 0;
  req->jamBufferPtr = jamBuffer();

  EXECUTE_DIRECT_MT(DBDIH, GSN_DIH_SCAN_TAB_REQ, signal,
                    DihScanTabReq::SignalLength, 0);

  DihScanTabConf *conf = (DihScanTabConf *)signal->getDataPtr();
  Uint32 senderData = conf->senderData;
  conf->senderData = treeNodePtr.i;
  /**
   * We need to introduce real-time break here for 2 reasons. The first
   * is that it is required by real-time break rules. We can start an
   * arbitrary number of prepare scans here. So it is necessary to do a
   * real-time break here to ensure that we don't execute for too long
   * without real-time breaks.
   *
   * The second reason is that the caller is looping over the list
   * of tree nodes and so we can't change this list while he is
   * looping over it. So we introduce a real-time break to ensure that
   * the caller only starts up prepare messages and don't actually
   * perform all of them.
   */
  if (senderData == 0) {
    sendSignal(reference(), GSN_DIH_SCAN_TAB_CONF, signal,
               DihScanTabConf::SignalLength, JBB);
    return;
  } else {
    sendSignal(reference(), GSN_DIH_SCAN_TAB_REF, signal,
               DihScanTabRef::SignalLength, JBB);
    return;
  }
}

void Dbspj::execDIH_SCAN_TAB_REF(Signal *signal) {
  jamEntry();
  DihScanTabRef *ref = (DihScanTabRef *)signal->getDataPtr();

  Ptr<TreeNode> treeNodePtr;
  ndbrequire(getGuardedPtr(treeNodePtr, ref->senderData));
  Ptr<Request> requestPtr;
  ndbrequire(m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));

  ndbrequire(requestPtr.p->isScan());
  ndbrequire(requestPtr.p->m_outstanding >= 1);
  requestPtr.p->m_outstanding -= 1;
  Uint32 errCode = ref->error;
  abort(signal, requestPtr, errCode);
}

void Dbspj::execDIH_SCAN_TAB_CONF(Signal *signal) {
  jamEntry();
  DihScanTabConf *conf = (DihScanTabConf *)signal->getDataPtr();

  Ptr<TreeNode> treeNodePtr;
  ndbrequire(getGuardedPtr(treeNodePtr, conf->senderData));
  ndbrequire(treeNodePtr.p->m_info == &g_ScanFragOpInfo);

  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;

  Uint32 cookie = conf->scanCookie;
  Uint32 fragCount = conf->fragmentCount;

  if (conf->reorgFlag) {
    jam();
    ScanFragReq *dst = reinterpret_cast<ScanFragReq *>(data.m_scanFragReq);
    ScanFragReq::setReorgFlag(dst->requestInfo, ScanFragReq::REORG_NOT_MOVED);
  }
  if (treeNodePtr.p->m_bits & TreeNode::T_CONST_PRUNE) {
    jam();
    fragCount = 1;
  }
  data.m_fragCount = fragCount;
  data.m_scanCookie = cookie;

  const Uint32 prunemask = TreeNode::T_PRUNE_PATTERN | TreeNode::T_CONST_PRUNE;
  bool pruned = (treeNodePtr.p->m_bits & prunemask) != 0;

  TableRecordPtr tablePtr;
  tablePtr.i = treeNodePtr.p->m_tableOrIndexId;
  ptrCheckGuard(tablePtr, c_tabrecFilesize, m_tableRecord);
  const bool readBackup =
      (tablePtr.p->m_flags & TableRecord::TR_READ_BACKUP) != 0;

  Ptr<Request> requestPtr;
  ndbrequire(m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));
  ndbassert(requestPtr.p->m_outstanding > 0);
  requestPtr.p->m_outstanding--;

  // Add a skew in the fragment lists such that we don't scan
  // the same subset of frags from all SPJ requests in case of
  // the scan not being 'T_SCAN_PARALLEL'
  const Uint16 fragNoOffs =
      (requestPtr.p->m_rootFragId * requestPtr.p->m_rootFragCnt) % fragCount;
  Uint32 err = 0;

  do {
    Ptr<ScanFragHandle> fragPtr;

    /** Allocate & init all 'fragCnt' fragment descriptors */
    {
      Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);

      err = checkTableError(treeNodePtr);
      if (unlikely(err != 0)) {
        jam();
        break;
      }
      for (Uint32 i = 0; i < fragCount; i++) {
        Ptr<ScanFragHandle> fragPtr;
        Uint16 fragNo = (fragNoOffs + i) % fragCount;

        if (!ERROR_INSERTED_CLEAR(17012) &&
            likely(
                m_scanfraghandle_pool.seize(requestPtr.p->m_arena, fragPtr))) {
          jam();
          fragPtr.p->init(fragNo, readBackup);
          fragPtr.p->m_treeNodePtrI = treeNodePtr.i;
          list.addLast(fragPtr);
          insertGuardedPtr(requestPtr, fragPtr);
        } else {
          jam();
          err = DbspjErr::OutOfQueryMemory;
          goto error;
        }
      }
      list.first(fragPtr);  // Needed if T_CONST_PRUNE
    }                       // end 'Alloc scope'

    if (treeNodePtr.p->m_bits & TreeNode::T_CONST_PRUNE) {
      jam();

      // TODO we need a different variant of computeHash here,
      // since m_constPrunePtrI does not contain full primary key
      // but only parts in distribution key

      BuildKeyReq tmp;
      Uint32 tableId = treeNodePtr.p->m_primaryTableId;
      err = computePartitionHash(signal, tmp, tableId, data.m_constPrunePtrI);
      if (unlikely(err != 0)) {
        jam();
        break;
      }

      releaseSection(data.m_constPrunePtrI);
      data.m_constPrunePtrI = RNIL;

      err = getNodes(signal, tmp, tableId);
      if (unlikely(err != 0)) {
        jam();
        break;
      }

      fragPtr.p->m_fragId = tmp.fragId;
      fragPtr.p->m_ref = tmp.receiverRef;
      fragPtr.p->m_next_ref = tmp.receiverRef;
      ndbassert(data.m_fragCount == 1);
    } else if (fragCount == 1) {
      jam();
      /**
       * This is roughly equivalent to T_CONST_PRUNE
       *   pretend that it is const-pruned
       */
      if (treeNodePtr.p->m_bits & TreeNode::T_PRUNE_PATTERN) {
        jam();
        LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                                   m_dependency_map_pool);
        Local_pattern_store pattern(pool, data.m_prunePattern);
        pattern.release();
      }
      data.m_constPrunePtrI = RNIL;
      Uint32 clear = TreeNode::T_PRUNE_PATTERN | TreeNode::T_SCAN_PARALLEL;
      treeNodePtr.p->m_bits &= ~clear;
      treeNodePtr.p->m_bits |= TreeNode::T_CONST_PRUNE;

      /**
       * We must get fragPtr.p->m_ref...so set pruned=false
       */
      pruned = false;
    }
    data.m_frags_complete = data.m_fragCount;

    if (!pruned) {
      /** Start requesting node info from DIH */
      jam();
      ndbassert(data.m_frags_outstanding == 0);
      data.m_frags_outstanding = data.m_fragCount;
      requestPtr.p->m_outstanding++;

      err = scanFrag_sendDihGetNodesReq(signal, requestPtr, treeNodePtr);
      if (unlikely(err != 0)) {
        jam();
        break;
      }
    } else {
      jam();
      treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;
    }

    ndbassert(err == 0);
    checkPrepareComplete(signal, requestPtr);
    return;
  } while (0);

error:
  jam();
  ndbassert(err != 0);
  abort(signal, requestPtr, err);
  checkBatchComplete(signal, requestPtr);
}

/**
 * Will check the fragment list for fragments which need to
 * get node info to construct 'fragPtr.p->m_ref' from DIH.
 *
 * In order to avoid CPU starvation, or unmanagable huge FragItem[],
 * max MAX_DIH_FRAG_REQS are requested in a single signal.
 * If there are more fragments, we have to repeatable call this
 * function when CONF for the first fragment set is received.
 */
Uint32 Dbspj::scanFrag_sendDihGetNodesReq(Signal *signal,
                                          Ptr<Request> requestPtr,
                                          Ptr<TreeNode> treeNodePtr) {
  jam();
  Uint32 err = 0;
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  Uint32 tableId = treeNodePtr.p->m_tableOrIndexId;
  TableRecordPtr tablePtr;
  Ptr<ScanFragHandle> fragPtr;
  Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, c_tabrecFilesize, m_tableRecord);
  Uint32 readAny =
      tablePtr.p->m_flags & TableRecord::TR_FULLY_REPLICATED ? 1 : 0;

  ndbassert(data.m_frags_outstanding > 0);

  Uint32 fragCnt = 0;
  for (list.first(fragPtr); !fragPtr.isNull(); list.next(fragPtr)) {
    jam();
    ndbassert(requestPtr.p->m_outstanding > 0);
    ndbassert(data.m_frags_outstanding > 0);

    if (fragCnt >= DiGetNodesReq::MAX_DIGETNODESREQS ||
        (ERROR_INSERTED(17131) && fragCnt >= 1)) {
      jam();
      signal->theData[0] = 3;
      signal->theData[1] = treeNodePtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      break;  // to exit
    }

    if (fragPtr.p->m_ref == 0)  // Need GSN_DIGETNODRESREQ
    {
      jam();
      DiGetNodesReq *const req = (DiGetNodesReq *)&signal->theData[0];

      req->tableId = treeNodePtr.p->m_tableOrIndexId;
      req->hashValue = fragPtr.p->m_fragId;
      req->distr_key_indicator = ZTRUE;
      req->scan_indicator = ZTRUE;
      req->anyNode = readAny;
      req->get_next_fragid_indicator = 0;
      req->jamBufferPtr = jamBuffer();

      EXECUTE_DIRECT_MT(DBDIH, GSN_DIGETNODESREQ, signal,
                        DiGetNodesReq::SignalLength, 0);

      const Uint32 errCode = signal->theData[0];

      if (ERROR_INSERTED_CLEAR(17130) && requestPtr.p->m_outstanding == 1) {
        jamEntry();
        data.m_frags_outstanding = 0;
        err = DbspjErr::OutOfSectionMemory;
        break;
      } else if (unlikely(errCode)) {
        jamEntry();
        data.m_frags_outstanding = 0;
        err = errCode;
        break;
      }

      const DiGetNodesConf *conf = (DiGetNodesConf *)&signal->theData[0];
      // if (!errCode)
      {
        /**
         * Get instance key from upper bits except most significant bit which
         * is used reorg moving flag.
         */
        jamEntry();
        /* Node cnt from DIH ignores primary, presumably to fit in 2 bits */
        Uint32 cnt = (conf->reqinfo & 3) + 1;
        Uint32 instanceKey = conf->instanceKey;
        ndbrequire(instanceKey > 0);
        NodeId nodeId = conf->nodes[0];
        if (nodeId != getOwnNodeId() && fragPtr.p->m_readBackup) {
          for (Uint32 i = 1; i < cnt; i++) {
            jam();
            if (conf->nodes[i] == getOwnNodeId()) {
              jam();
              nodeId = getOwnNodeId();
              break;
            }
          }
          if (nodeId != getOwnNodeId()) {
            Uint32 node;
            jam();
            if ((node = check_own_location_domain(&conf->nodes[0], cnt)) != 0) {
              nodeId = node;
            }
          }
        }
        Uint32 instanceNo = getInstanceNo(nodeId, instanceKey);
        Uint32 blockNo = get_query_block_no(nodeId);
        fragPtr.p->m_ref = numberToRef(blockNo, instanceNo, nodeId);
        fragPtr.p->m_next_ref = fragPtr.p->m_ref;
        /**
         * For Fully replicated tables we can change the fragment id to a local
         * fragment as part of DIGETNODESREQ. So set it again here.
         */
        fragPtr.p->m_fragId = conf->fragId;
      }

      fragCnt++;
      ndbassert(data.m_frags_outstanding > 0);
      ndbassert(treeNodePtr.p->m_state != TreeNode::TN_INACTIVE);
      data.m_frags_outstanding--;
    }
  }
  jam();

  if (data.m_frags_outstanding == 0) {
    jam();
    treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;
    requestPtr.p->m_outstanding--;
  }
  return err;
}  // Dbspj::scanFrag_sendDihGetNodesReq

void Dbspj::scanFrag_start(Signal *signal, Ptr<Request> requestPtr,
                           Ptr<TreeNode> treeNodePtr) {
  jam();
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;

  ndbassert(data.m_fragCount > 0);
  ndbassert(data.m_frags_outstanding == 0);
  ndbassert(data.m_frags_complete == 0);
  data.m_frags_not_started = data.m_fragCount;

  ndbassert(treeNodePtr.p->m_state == TreeNode::TN_BUILDING);
  treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;

  scanFrag_send(signal, requestPtr, treeNodePtr);
  // Register node which started the scan (reflect client expectations)
  requestPtr.p->m_active_tree_nodes.set(treeNodePtr.p->m_node_no);
}  // Dbspj::scanFrag_start

Uint32 Dbspj::scanFrag_findFrag(Local_ScanFragHandle_list &list,
                                Ptr<ScanFragHandle> &fragPtr, Uint32 fragId) {
  for (list.first(fragPtr); !fragPtr.isNull(); list.next(fragPtr)) {
    jam();
    if (fragPtr.p->m_fragId == fragId) {
      jam();
      return 0;
    }
  }

  return DbspjErr::IndexFragNotFound;
}

void Dbspj::scanFrag_parent_row(Signal *signal, Ptr<Request> requestPtr,
                                Ptr<TreeNode> treeNodePtr,
                                const RowPtr &rowRef) {
  jam();
  ndbassert(treeNodePtr.p->m_parentPtrI != RNIL);
  DEBUG("::scanFrag_parent_row"
        << ", node: " << treeNodePtr.p->m_node_no);

  Uint32 err;
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;

  /**
   * Construct range definition,
   *   and if prune pattern enabled
   *   stuff it onto correct scanFrag
   */
  do {
    Ptr<ScanFragHandle> fragPtr;
    Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
    LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                               m_dependency_map_pool);

    err = checkTableError(treeNodePtr);
    if (unlikely(err != 0)) {
      jam();
      break;
    }

    if (treeNodePtr.p->m_bits & TreeNode::T_PRUNE_PATTERN) {
      jam();

      /**
       * TODO: Expand into linear memory instead
       *       of expanding into sections, and then copy
       *       section into linear
       */
      Local_pattern_store pattern(pool, data.m_prunePattern);
      Uint32 pruneKeyPtrI = RNIL;
      bool hasNull;
      err = expand(pruneKeyPtrI, pattern, rowRef, hasNull);
      if (unlikely(err != 0)) {
        jam();
        releaseSection(pruneKeyPtrI);
        break;
      }

      if (unlikely(hasNull)) {
        jam();
        DEBUG("T_PRUNE_PATTERN-key contain NULL values");

        // Ignore this request as 'NULL == <column>' will never give a match
        releaseSection(pruneKeyPtrI);
        return;  // Bailout, SCANREQ would have returned 0 rows anyway
      }

      BuildKeyReq tmp;
      Uint32 tableId = treeNodePtr.p->m_primaryTableId;
      err = computePartitionHash(signal, tmp, tableId, pruneKeyPtrI);
      releaseSection(pruneKeyPtrI);
      if (unlikely(err != 0)) {
        jam();
        break;
      }

      err = getNodes(signal, tmp, tableId);
      if (unlikely(err != 0)) {
        jam();
        break;
      }

      err = scanFrag_findFrag(list, fragPtr, tmp.fragId);
      if (unlikely(err != 0)) {
        DEBUG_CRASH();
        break;
      }

      /**
       * NOTE: We can get different receiverRef's here
       *       for different keys. E.g during node-recovery where
       *       primary-fragment is switched.
       *
       *       Use latest that we receive
       *
       * TODO: Also double check table-reorg
       */
      fragPtr.p->m_ref = tmp.receiverRef;
      fragPtr.p->m_next_ref = tmp.receiverRef;
    } else {
      jam();
      /**
       * If const prune, or no-prune, store on first fragment,
       * and send to 1 or all resp.
       */
      list.first(fragPtr);
    }

    if (treeNodePtr.p->m_bits & TreeNode::T_KEYINFO_CONSTRUCTED) {
      jam();
      Local_pattern_store pattern(pool, treeNodePtr.p->m_keyPattern);

      /**
       * Test execution terminated due to 'OutOfSectionMemory':
       * - 17060: Fail on scanFrag_parent_row at first call
       * - 17061: Fail on scanFrag_parent_row if 'isLeaf'
       * - 17062: Fail on scanFrag_parent_row if treeNode not root
       * - 17063: Fail on scanFrag_parent_row at a random node of the query tree
       */
      if (ERROR_INSERTED(17060) ||
          (ERROR_INSERTED(17061) && (treeNodePtr.p->isLeaf())) ||
          (ERROR_INSERTED(17062) && (treeNodePtr.p->m_parentPtrI != RNIL)) ||
          (ERROR_INSERTED(17063) && (rand() % 7) == 0)) {
        jam();
        CLEAR_ERROR_INSERT_VALUE;
        g_eventLogger->info(
            "Injecting OutOfSectionMemory error at line %d file %s", __LINE__,
            __FILE__);
        err = DbspjErr::OutOfSectionMemory;
        break;
      }

      bool hasNull = false;
      Uint32 keyPtrI = RNIL;
      err = expand(keyPtrI, pattern, rowRef, hasNull);
      if (unlikely(err != 0)) {
        jam();
        break;
      }
      if (hasNull) {
        jam();
        DEBUG("Key contain NULL values, ignoring it");
        ndbassert((treeNodePtr.p->m_bits & TreeNode::T_ONE_SHOT) == 0);
        // Ignore this request as 'NULL == <column>' will never give a match
        releaseSection(keyPtrI);
        return;  // Bailout, SCANREQ would have returned 0 rows anyway
      }
      scanFrag_fixupBound(keyPtrI, rowRef.m_src_correlation);

      SectionReader key(keyPtrI, getSectionSegmentPool());
      err = appendReaderToSection(fragPtr.p->m_rangePtrI, key, key.getSize());
      fragPtr.p->m_rangeCnt++;
      releaseSection(keyPtrI);
      if (unlikely(err != 0)) {
        jam();
        break;
      }
    } else {
      jam();
      // Fixed key...fix later...
      ndbabort();
    }

    if (treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED) {
      jam();
      // Append to fragPtr's parameter set
      // Build with the 'attrParamPattern' applied to the parent rowRef
      DEBUG("parent_row w/ T_ATTRINFO_CONSTRUCTED");
      Uint32 paramPtrI = fragPtr.p->m_paramPtrI;
      Uint32 org_size = 0;
      if (paramPtrI != RNIL) {
        // Get current end of parameter section
        SegmentedSectionPtr ptr;
        getSection(ptr, paramPtrI);
        org_size = ptr.sz;
      }
      Uint32 paramLen = 0;  // Set paramLen after it has been expanded
      if (unlikely(!appendToSection(paramPtrI, &paramLen, 1))) {
        jam();
        err = DbspjErr::OutOfSectionMemory;
        break;
      }
      bool hasNull = false;
      Local_pattern_store pattern(pool, treeNodePtr.p->m_attrParamPattern);
      err = expand(paramPtrI, pattern, rowRef, hasNull);
      if (unlikely(err != 0)) {
        jam();
        break;
      }

      /**
       * Set size of this parameter. Note that parameter 'isNull' is OK.
       */
      {
        SegmentedSectionPtr ptr;
        getSection(ptr, paramPtrI);
        paramLen = ptr.sz - org_size;
      }
      writeToSection(paramPtrI, org_size, &paramLen, 1);
      fragPtr.p->m_paramPtrI = paramPtrI;
    }

    if (treeNodePtr.p->m_bits & TreeNode::T_ONE_SHOT) {
      jam();
      /**
       * We being a T_ONE_SHOT means that we're only be called
       *   with parent_row once, i.e batch is complete
       */
      scanFrag_parent_batch_complete(signal, requestPtr, treeNodePtr);
    }

    return;
  } while (0);

  ndbrequire(err);
  jam();
  abort(signal, requestPtr, err);
}

void Dbspj::scanFrag_fixupBound(Uint32 ptrI, Uint32 corrVal) {
  /**
   * Index bounds...need special tender and care...
   *
   * 1) Set #bound no, bound-size, and renumber attributes
   */
  SectionReader r0(ptrI, getSectionSegmentPool());
  const Uint32 boundsz = r0.getSize();

  Uint32 tmp;
  ndbrequire(r0.peekWord(&tmp));
  ndbassert((corrVal & 0xFFFF) < MaxCorrelationId);
  tmp |= (boundsz << 16) | ((corrVal & 0xFFF) << 4);
  ndbrequire(r0.updateWord(tmp));
  ndbrequire(r0.step(1));  // Skip first BoundType

  // Note: Renumbering below assume there are only EQ-bounds !!
  Uint32 id = 0;
  Uint32 len32;
  do {
    ndbrequire(r0.peekWord(&tmp));
    AttributeHeader ah(tmp);
    const Uint32 len = ah.getByteSize();
    AttributeHeader::init(&tmp, id++, len);
    ndbrequire(r0.updateWord(tmp));
    len32 = (len + 3) >> 2;
  } while (r0.step(2 + len32));  // Skip AttributeHeader(1) + Attribute(len32) +
                                 // next BoundType(1)
}

void Dbspj::scanFrag_parent_batch_complete(Signal *signal,
                                           Ptr<Request> requestPtr,
                                           Ptr<TreeNode> treeNodePtr) {
  jam();
  ndbassert(treeNodePtr.p->m_parentPtrI != RNIL);
  ndbassert(treeNodePtr.p->m_state == TreeNode::TN_INACTIVE);

  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  ndbassert(data.m_frags_complete == data.m_fragCount);

  /**
   * Update the fragments 'm_state' and the aggregated TreeNode::m_frag_*
   * counters to reflect which fragments we should now start scanning.
   * NOTE: 'm_state' is not maintained if all 'complete' - node becomes
   * inactive
   */
  {
    Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
    Ptr<ScanFragHandle> fragPtr;
    list.first(fragPtr);
    data.m_frags_complete = 0;

    if ((treeNodePtr.p->m_bits & TreeNode::T_PRUNE_PATTERN) == 0) {
      /* No pruning, first fragment in list contains any range info */
      if (fragPtr.p->m_rangePtrI != RNIL) {
        /* All fragments to be scanned with range info */
        while (!fragPtr.isNull()) {
          ndbassert(fragPtr.p->m_state == ScanFragHandle::SFH_NOT_STARTED ||
                    fragPtr.p->m_state == ScanFragHandle::SFH_COMPLETE);
          fragPtr.p->m_state = ScanFragHandle::SFH_NOT_STARTED;
          list.next(fragPtr);
        }
      } else {
        /* No range info therefore empty result set. */
        jam();
        data.m_frags_complete = data.m_fragCount;
      }
    } else {
      /* Per fragment pruning, mark and count pruned-out
       * (rangeless) fragments as completed
       */
      while (!fragPtr.isNull()) {
        fragPtr.p->m_state = ScanFragHandle::SFH_NOT_STARTED;
        if (fragPtr.p->m_rangePtrI == RNIL) {
          jam();
          /**
           * This is a pruned scan, so we only scan those fragments that
           * some distribution key hashed to.
           */
          fragPtr.p->m_state = ScanFragHandle::SFH_COMPLETE;
          data.m_frags_complete++;
        }
        list.next(fragPtr);
      }
    }
    data.m_frags_not_started = data.m_fragCount - data.m_frags_complete;
  }

  if (data.m_frags_complete == data.m_fragCount) {
    jam();
    /**
     * No keys was produced...
     */
    return;
  }

  /**
   * When parent's batch is complete, we send our batch
   */
  scanFrag_send(signal, requestPtr, treeNodePtr);
}

/**
 * scanFrag_getBatchSize()
 *
 * Estimate how many more rows we may fetch into the available client batch
 * buffers, given the configured 'bytes' and 'rows' available. We are also
 * limited by the 12-bit correlation id range. Note that some 'bytes', 'rows'
 * and 'correlationIds' may already have been used up as reflected in 'data'.
 *
 * The collected 'record size'-statistic is used to estimate how many 'rows'
 * we may fit in the available batch bytes. The most restrictive limitation
 * of 'bytes', 'rows' and 'correlationId' is used to calculate the 'BatchSize'
 * we may use
 *
 * Sets the remaining 'available' bytes/rows upon return.
 */
Uint32 Dbspj::scanFrag_getBatchSize(Ptr<TreeNode> treeNodePtr,
                                    Uint32 &availableBatchBytes,
                                    Uint32 &availableBatchRows) {
  jam();
  const ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  const ScanFragReq *org =
      reinterpret_cast<const ScanFragReq *>(data.m_scanFragReq);

  const Uint32 availableCorrIds = MaxCorrelationId - data.m_corrIdStart;
  /**
   * Available 'Batch Rows' is limited both by the max specified
   * batch size, and for any node being a parent node, also the
   * max number of 12-bit parent correlation ids
   */
  availableBatchRows =
      (treeNodePtr.p->isLeaf())
          ? (org->batch_size_rows - data.m_totalRows)
          : MIN((org->batch_size_rows - data.m_totalRows), availableCorrIds);
  availableBatchBytes = org->batch_size_bytes - data.m_totalBytes;

  /**
   * Number of rows in batch could effectively be limited by
   * the 'bytes' limit being exhausted first.
   */
  Uint32 batchSizeRows = availableBatchRows;
  if (data.m_recSizeStat.isValid()) {
    const double estmRecSize = data.m_recSizeStat.getUpperEstimate();
    const Uint32 batchLimitedByBytes = availableBatchBytes / estmRecSize;
    if (batchSizeRows > batchLimitedByBytes)
      batchSizeRows = batchLimitedByBytes;
  }
  return batchSizeRows;
}

/**
 * scanFrag_parallelism()
 *
 * Calculate the fragment parallelism to be used in the next scan
 * when the estimated number of 'batchSizeRows' can be returned to the
 * client side.
 * Use scanFrag_getBatchSize() to calculate 'batchSizeRows'.
 *
 * Return parallelism = 0 if parallelism could not be estimated.
 */
Uint32 Dbspj::scanFrag_parallelism(Ptr<Request> requestPtr,
                                   Ptr<TreeNode> treeNodePtr,
                                   Uint32 batchSizeRows) {
  jam();
  ndbassert(batchSizeRows > 0);
  if (unlikely(batchSizeRows == 0)) return 0;  // Should not happen

  const ScanFragData &data = treeNodePtr.p->m_scanFrag_data;

  const Uint32 frags_not_complete = data.m_fragCount - data.m_frags_complete;
  const Uint32 maxParallelism = MIN(batchSizeRows, frags_not_complete);
  const Uint32 minParallelism =
      MIN(requestPtr.p->m_rootFragCnt, maxParallelism);

  if (treeNodePtr.p->m_bits & TreeNode::T_SCAN_PARALLEL) {
    jam();
    return maxParallelism;
  }
  if (!data.m_recsPrKeyStat.isValid()) {
    jam();
    return minParallelism;
  }

  /**
   * We usually use 'Record Pr Key' fanout statistics from previous runs
   * of this operation to estimate a parallelism for the fragment scans.
   *
   * Upper 95% percentile of estimated rows to be returned is calculated
   * and transformed into 'parallelism', given the available batch size.
   * Note that we prefer erring with a too low parallelism, as we else
   * would have to send more NEXTREQs to the fragment which didn't
   * complete in this round. (Which would have been more costly)
   */
  const double estmRecsPrKey = data.m_recsPrKeyStat.getUpperEstimate();
  const Uint32 estmRowsSelected =
      MAX(static_cast<Uint32>(data.m_keysToSend * estmRecsPrKey), 1);
  Uint32 parallelism =
      (batchSizeRows * data.m_frags_not_started) / estmRowsSelected;

  if (parallelism < minParallelism) {
    parallelism = minParallelism;
  } else if (parallelism >= maxParallelism) {
    parallelism = maxParallelism;
  } else if (maxParallelism % parallelism != 0) {
    /**
     * Set parallelism such that we can expect to have similar
     * parallelism in each batch. For example if there are 8 remaining
     * fragments, then we should fetch 2 times 4 fragments rather than
     * 7+1.
     * Note this this might result in 'parallelism < minParallelism'.
     * minParallelism is not a hard limit though, so it is OK
     */
    const Uint32 roundTrips = 1 + (maxParallelism / parallelism);
    parallelism = (maxParallelism + roundTrips - 1) / roundTrips;
  }

  /**
   * Parallelism must be increased if we otherwise would be limited
   * by the MAX_PARALLEL_OP_PER_SCAN limitation in the SCAN_FRAGREQs
   */
  const ScanFragReq *req =
      reinterpret_cast<const ScanFragReq *>(data.m_scanFragReq);
  const Uint32 availableBatchRows = req->batch_size_rows - data.m_totalRows;

  ndbrequire(availableBatchRows >= batchSizeRows);
  if ((availableBatchRows / parallelism) > MAX_PARALLEL_OP_PER_SCAN) {
    jam();
    parallelism = MIN((availableBatchRows + MAX_PARALLEL_OP_PER_SCAN - 1) /
                          MAX_PARALLEL_OP_PER_SCAN,
                      data.m_frags_not_started);
  }

  return parallelism;
}  // scanFrag_parallelism

/**
 * Estimate how many keys we can supply in a REQuest to the treeNode branch
 * before overflowing the available batch buffer space in any of the
 * (child-) treenodes in the branch. For non pruned scans we asssume that
 *  fragments scan requests are sent to all of the fragments.
 */
double Dbspj::estmMaxKeys(Ptr<Request> requestPtr, Ptr<TreeNode> treeNodePtr,
                          double fanout) {
  double maxKeys = 99999.99;

  if (treeNodePtr.p->isScan()) {
    /**
     * Multiply the fanout with 'records pr key' estimate for this treeNode.
     * Note that for a lookup the 'records pr key' is assumed to be 1::1.
     * (It can be lower, we do not collect that statistic (yet) though)
     */
    const ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
    if (data.m_recsPrKeyStat.isValid()) {
      if (treeNodePtr.p->isLeaf())
        fanout *= data.m_recsPrKeyStat.getUpperEstimate();
      else
        fanout *= data.m_recsPrKeyStat.getMean();
    }
    const bool pruned = treeNodePtr.p->m_bits &
                        (TreeNode::T_PRUNE_PATTERN | TreeNode::T_CONST_PRUNE);
    if (!pruned) {
      // Fanout statistics is pr. fragment we scan
      fanout *= data.m_fragCount;
    }

    Uint32 availableBatchRows, availableBatchBytes;  // Unused
    const Uint32 batchRows = scanFrag_getBatchSize(
        treeNodePtr, availableBatchBytes, availableBatchRows);
    maxKeys = static_cast<double>(batchRows) / fanout;
  }

  LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                             m_dependency_map_pool);
  Local_dependency_map const childList(pool, treeNodePtr.p->m_child_nodes);
  Dependency_map::ConstDataBufferIterator it;
  for (childList.first(it); !it.isNull(); childList.next(it)) {
    jam();
    Ptr<TreeNode> childPtr;
    ndbrequire(m_treenode_pool.getPtr(childPtr, *it.data));

    const double estmKeys = estmMaxKeys(requestPtr, childPtr, fanout);
    if (estmKeys < maxKeys) maxKeys = estmKeys;
  }
  return maxKeys;
}

void Dbspj::scanFrag_send(Signal *signal, Ptr<Request> requestPtr,
                          Ptr<TreeNode> treeNodePtr) {
  jam();
  ndbassert(treeNodePtr.p->m_state == TreeNode::TN_INACTIVE);
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  ndbassert(data.m_frags_outstanding == 0);
  ndbassert(data.m_frags_not_started ==
            (data.m_fragCount - data.m_frags_complete));

  /**
   * Sum up the total number of key ranges to request rows from when
   * scanning all the fragments we are going to retrieve rows from.
   * Later used together with the 'RecsPrKey' statistis to estimate number
   * of rows to be returned.
   */
  {
    const bool pruned = treeNodePtr.p->m_bits &
                        (TreeNode::T_PRUNE_PATTERN | TreeNode::T_CONST_PRUNE);

    Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
    Ptr<ScanFragHandle> fragPtr;
    list.first(fragPtr);

    Uint32 keysToSend = 0;
    if (!pruned) {
      /**
       * if not 'pruned', keyInfo is only set in first fragPtr,
       *   even if it is valid for all of them. (save some mem.)
       */
      keysToSend = fragPtr.p->m_rangeCnt * data.m_frags_not_started;
    } else  // Sum the total pruned scan keys to be sent
    {
      while (!fragPtr.isNull()) {
        keysToSend += fragPtr.p->m_rangeCnt;
        list.next(fragPtr);
      }
    }
    data.m_keysToSend = keysToSend;
  }

  data.m_rows_received = 0;
  data.m_rows_expecting = 0;

  Uint32 availableBatchRows, availableBatchBytes;
  const Uint32 batchRows = scanFrag_getBatchSize(
      treeNodePtr, availableBatchBytes, availableBatchRows);
  data.m_parallelism = scanFrag_parallelism(requestPtr, treeNodePtr, batchRows);

  // Cap batchSize-rows to avoid exceeding MAX_PARALLEL_OP_PER_SCAN
  const Uint32 bs_rows =
      MIN(availableBatchRows / data.m_parallelism, MAX_PARALLEL_OP_PER_SCAN);
  const Uint32 bs_bytes = availableBatchBytes / data.m_parallelism;
  ndbassert(bs_rows > 0);
  ndbassert(bs_bytes > 0);

#ifdef DEBUG_SCAN_FRAGREQ
  DEBUG("::scanFrag_send(), starting fragment scan with parallelism="
        << data.m_parallelism);
#endif

  Uint32 frags_started = scanFrag_send(signal, requestPtr, treeNodePtr,
                                       data.m_parallelism, bs_bytes, bs_rows);

  /**
   * scanFrag_send might fail to send (errors?):
   * Check that we really did send something before
   * updating outstanding & active.
   */
  if (likely(frags_started > 0)) {
    jam();
    ndbrequire(static_cast<Uint32>(data.m_frags_outstanding +
                                   data.m_frags_complete) <= data.m_fragCount);

    data.m_batch_chunks = 1;
    requestPtr.p->m_cnt_active++;
    requestPtr.p->m_outstanding++;
    requestPtr.p->m_completed_tree_nodes.clear(treeNodePtr.p->m_node_no);
    treeNodePtr.p->m_state = TreeNode::TN_ACTIVE;
  }
}

/**
 * Ask for the first batch for a number of fragments.
 *
 * Returns how many fragments we did request the
 * 'first batch' from. (<= noOfFrags)
 */
Uint32 Dbspj::scanFrag_send(Signal *signal, Ptr<Request> requestPtr,
                            Ptr<TreeNode> treeNodePtr, Uint32 noOfFrags,
                            Uint32 bs_bytes, Uint32 bs_rows) {
  jam();
  ndbassert(bs_bytes > 0);
  ndbassert(bs_rows > 0);
  ndbassert(bs_rows <= bs_bytes);
  /**
   * if (m_bits & prunemask):
   * - Range keys sliced out to each ScanFragHandle
   * - Else, range keys kept on first (and only) ScanFragHandle
   */
  const bool prune = treeNodePtr.p->m_bits &
                     (TreeNode::T_PRUNE_PATTERN | TreeNode::T_CONST_PRUNE);

  /**
   * If scan is repeatable, we must make sure not to release range keys so
   * that we can use them again in the next repetition.
   */
  const bool repeatable =
      (treeNodePtr.p->m_bits & TreeNode::T_SCAN_REPEATABLE) != 0;

  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  ndbassert(noOfFrags > 0);
  ndbassert(noOfFrags <= data.m_frags_not_started);
  ScanFragReq *const req =
      reinterpret_cast<ScanFragReq *>(signal->getDataPtrSend());
  const ScanFragReq *const org =
      reinterpret_cast<ScanFragReq *>(data.m_scanFragReq);

  memcpy(req, org, sizeof(data.m_scanFragReq));
  // req->variableData[0] // set below
  req->variableData[1] = requestPtr.p->m_rootResultData;
  req->batch_size_bytes = bs_bytes;
  req->batch_size_rows = MIN(bs_rows, MAX_PARALLEL_OP_PER_SCAN);

  /**
   * A SORTED_ORDER scan need to fetch one row at a time from the treeNode
   * to be ordered - See reasoning where we set the T_SORTED_ORDER bit.
   */
  if (treeNodePtr.p->m_bits & TreeNode::T_SORTED_ORDER &&
      requestPtr.p->m_bits & Request::RT_MULTI_SCAN) {
    jam();
    req->batch_size_rows = bs_rows = 1;
    ndbrequire(data.m_parallelism == 1);
  }

  Uint32 requestsSent = 0;
  Uint32 err = checkTableError(treeNodePtr);
  if (likely(err == 0)) {
    Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
    Ptr<ScanFragHandle> fragPtr;
    list.first(fragPtr);
    bool handleLocalFrags = true;

    /**
     * Iterate over the list of fragments until we have sent as many
     * SCAN_FRAGREQs as we should.
     */
    while (requestsSent < noOfFrags) {
      jam();
      if (handleLocalFrags) {
        if (fragPtr.isNull()) {
          // We might have skipped to end of the fragment list while first
          // sending requests to only the local fragments, start over
          handleLocalFrags = false;
          list.first(fragPtr);
          continue;
        }
        if (refToNode(fragPtr.p->m_ref) != getOwnNodeId()) {
          // Skip non local fragments
          list.next(fragPtr);
          continue;
        }
      }
      ndbassert(!fragPtr.isNull());

      /**
       * There is a 12-bit implementation limit on how large
       * the 'parent-row-correlation-id' may be. Thus, if rows
       * from this scan may be 'parents', number of rows in batch
       * should not exceed what could be represented in 12 bits.
       * See also Dbspj::scanFrag_fixupBound()
       */
      ndbassert(treeNodePtr.p->isLeaf() ||
                data.m_corrIdStart + bs_rows <= MaxCorrelationId);

      if (fragPtr.p->m_state != ScanFragHandle::SFH_NOT_STARTED) {
        // Skip forward to the frags that we should send.
        jam();
        list.next(fragPtr);
        continue;
      }

      /**
       * Set data specific for this fragment
       */
      req->senderData = fragPtr.i;
      req->fragmentNoKeyLen = fragPtr.p->m_fragId;
      req->variableData[0] = data.m_corrIdStart;

      /**
       * Set up the key-/attrInfo to be sent with the SCAN_FRAGREQ.
       * Determine whether these should released as part of the
       * send or not. We try to 'release' whenever possible in order
       * to avoid copying them when sent locally. However, we need
       * to make sure that the key/attr will not be reused before
       * they can be released. Note:
       *
       * - Only the rootNode is ONE_SHOT.
       * - keyInfo comes from either m_send.m_keyInfoPtrI or
       *   fragPtr.p->m_rangePtrI (not both! - 'XOR').
       * - If the child scan is pruned, a separate 'rangePtr' is
       *   build for each frag - Non-pruned scan store the 'rangePtr'
       *   in the first frag, which is reused for all the frags.
       * - Child nodes can possibly be 'repeatable', which implies
       *   that m_rangePtrI can't be released yet.
       * - attrInfo is always taken from m_send.m_attrInfoPtrI, possibly
       *   with constructed parameters appended. It is reused from
       *   all frag scans, either repeated or not!
       *
       * Note the somewhat different lifetime of key- vs attrInfo:
       * Except for the ONE_SHOT rootNode, the attrInfo always has
       * to be kept longer than 'key' before released.
       * As sendSignal() either release both or none, we can't
       * set 'releaseAtSend' to suite both key- and attrInfo
       * lifetime.
       *
       * Thus, we set 'releaseAtSend' to suite the shorter lifecycle
       * of the 'range' keys. attrInfo is duplicated whenever needed
       * such that a copy can be released together with the keyInfo.
       */
      Uint32 attrInfoPtrI = treeNodePtr.p->m_send.m_attrInfoPtrI;
      Uint32 keyInfoPtrI = treeNodePtr.p->m_send.m_keyInfoPtrI;
      bool releaseAtSend = false;

      if (treeNodePtr.p->m_bits & TreeNode::T_ONE_SHOT &&
          data.m_frags_not_started == 1) {
        jam();
        ndbassert(!repeatable);
        ndbassert(fragPtr.p->m_rangePtrI == RNIL);
        ndbassert(fragPtr.p->m_paramPtrI == RNIL);
        /**
         * Pass sections to send and release them (root only)
         */
        treeNodePtr.p->m_send.m_attrInfoPtrI = RNIL;
        treeNodePtr.p->m_send.m_keyInfoPtrI = RNIL;
        releaseAtSend = true;
      } else {
        jam();
        Ptr<ScanFragHandle> fragWithRangePtr;
        if (prune) {
          jam();
          fragWithRangePtr = fragPtr;
          releaseAtSend = !repeatable;
        } else {
          /**
           * Note: if not 'prune', keyInfo is only set in first fragPtr,
           *   even if it is valid for all of them. (save some mem.)
           */
          jam();
          list.first(fragWithRangePtr);
          releaseAtSend = (!repeatable && data.m_frags_not_started == 1);
        }
        if (fragWithRangePtr.p->m_rangePtrI != RNIL) {
          ndbassert(keyInfoPtrI == RNIL);  // Not both keyInfo and 'range'
          keyInfoPtrI = fragWithRangePtr.p->m_rangePtrI;
          fragPtr.p->m_keysSent = fragWithRangePtr.p->m_rangeCnt;
          data.m_keysToSend -= fragPtr.p->m_keysSent;
        }
        /**
         * 'releaseAtSend' is set above based on the keyInfo lifetime.
         * Copy the attrInfo (comment above) whenever needed.
         * If the attrInfo is constructed it has to be duplicated as well
         * in preparation for the parameter to be appended
         */
        if (releaseAtSend ||
            treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED) {
          jam();
          /**
           * Test execution terminated due to 'OutOfSectionMemory' which
           * may happen for different treeNodes in the request:
           * - 17090: Fail on any scanFrag_send()
           * - 17091: Fail after sending SCAN_FRAGREQ to some fragments
           * - 17092: Fail on scanFrag_send() if 'isLeaf'
           * - 17093: Fail on scanFrag_send() if treeNode not root
           */
          if (ERROR_INSERTED(17090) ||
              (ERROR_INSERTED(17091) && requestsSent > 1) ||
              (ERROR_INSERTED(17092) && treeNodePtr.p->isLeaf()) ||
              (ERROR_INSERTED(17093) && treeNodePtr.p->m_parentPtrI != RNIL)) {
            jam();
            CLEAR_ERROR_INSERT_VALUE;
            g_eventLogger->info(
                "Injecting OutOfSectionMemory error at line %d file %s",
                __LINE__, __FILE__);
            err = DbspjErr::OutOfSectionMemory;
            break;
          }
          Uint32 tmp = RNIL;
          if (!dupSection(tmp, attrInfoPtrI)) {
            jam();
            ndbassert(tmp == RNIL);  // Guard for memleak
            err = DbspjErr::OutOfSectionMemory;
            break;
          }
          attrInfoPtrI = tmp;
        }  // if (releaseAtSend || ATTRINFO_CONSTRUCTED)

        if (treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED) {
          jam();
          /**
           * We constructed a parameter section in scanFrag_parent_row(), append
           * it to the attrInfo as we send each fragment scans.
           */
          SectionReader params(fragWithRangePtr.p->m_paramPtrI,
                               getSectionSegmentPool());
          const Uint32 paramLen = params.getSize();
          err = appendReaderToSection(attrInfoPtrI, params, paramLen);
          if (unlikely(err != 0)) {
            jam();
            releaseSection(attrInfoPtrI);
            break;
          }
          SegmentedSectionPtr ptr;
          getSection(ptr, attrInfoPtrI);
          Uint32 *sectionptrs = ptr.p->theData;
          sectionptrs[4] = paramLen;
        }  // ATTRINFO_CONSTRUCTED

        if (releaseAtSend) {
          jam();
          /** Reflect the release of the keyInfo 'range' set above */
          fragWithRangePtr.p->m_rangePtrI = RNIL;
          fragWithRangePtr.p->m_rangeCnt = 0;

          if (fragWithRangePtr.p->m_paramPtrI != RNIL) {
            releaseSection(fragWithRangePtr.p->m_paramPtrI);
            fragWithRangePtr.p->m_paramPtrI = RNIL;
          }
        }  // if (releaseAtSend)
      }

      SectionHandle handle(this);
      getSection(handle.m_ptr[0], attrInfoPtrI);
      handle.m_cnt = 1;
      if (keyInfoPtrI != RNIL) {
        jam();
        getSection(handle.m_ptr[1], keyInfoPtrI);
        handle.m_cnt++;
      }

#if defined DEBUG_SCAN_FRAGREQ
      g_eventLogger->info("SCAN_FRAGREQ to %x", fragPtr.p->m_ref);
      printSCAN_FRAGREQ(
          stdout, signal->getDataPtrSend(),
          NDB_ARRAY_SIZE(treeNodePtr.p->m_scanFrag_data.m_scanFragReq), DBLQH);
      printf("ATTRINFO: ");
      print(handle.m_ptr[0], stdout);
      if (handle.m_cnt > 1) {
        printf("KEYINFO: ");
        print(handle.m_ptr[1], stdout);
      }
#endif

      Uint32 ref = fragPtr.p->m_ref;
      Uint32 nodeId = refToNode(ref);
      if (!ScanFragReq::getRangeScanFlag(req->requestInfo)) {
        c_Counters.incr_counter(CI_LOCAL_TABLE_SCANS_SENT, 1);
      } else if (nodeId == getOwnNodeId()) {
        c_Counters.incr_counter(CI_LOCAL_RANGE_SCANS_SENT, 1);
      } else {
        ndbrequire(!ERROR_INSERTED(17014));
        c_Counters.incr_counter(CI_REMOTE_RANGE_SCANS_SENT, 1);
      }

      /**
       * For a non-repeatable pruned scan, key info is unique for each
       * fragment and therefore cannot be reused, so we release key info
       * right away.
       */

      if (ERROR_INSERTED(17110) ||
          (ERROR_INSERTED(17111) && treeNodePtr.p->isLeaf()) ||
          (ERROR_INSERTED(17112) && treeNodePtr.p->m_parentPtrI != RNIL)) {
        jam();
        CLEAR_ERROR_INSERT_VALUE;
        g_eventLogger->info(
            "Injecting invalid schema version error at line %d file %s",
            __LINE__, __FILE__);
        // Provoke 'Invalid schema version' in order to receive SCAN_FRAGREF
        req->schemaVersion++;
      }

      Uint32 instance_no = refToInstance(ref);
      if (ScanFragReq::getNoDiskFlag(req->requestInfo)) {
        if (nodeId == getOwnNodeId()) {
          if (globalData.ndbMtQueryThreads > 0) {
            /**
             * ReadCommittedFlag is always set in DBSPJ when Query threads are
             * used.
             *
             * We have retrieved the block reference from fragPtr.p->m_ref.
             * This block reference might be reused for multiple scans, thus
             * we cannot update it. However we need to ensure that SCAN_NEXTREQ
             * is sent to the same block that this signal is sent to. This is
             * taken care of when SCAN_FRAGCONF/SCAN_FRAGREF is returned. In
             * this return message we get the block reference of the DBLQH/
             * DBQLQH that sent the signal. We store this value in m_next_ref
             * on the fragment record.
             *
             * Since the scheduling to the query thread happens here we will
             * set m_next_ref already here and have no need to wait for return
             * signal to set m_next_ref.
             *
             * m_next_ref is used in sending SCAN_NEXTREQ always, since this
             * must be sent after receiving a SCAN_FRAGCONF/SCAN_FRAGREF
             * signal the m_next_ref should always be set to something
             * proper.
             *
             * This way of handling things means that we are able to schedule
             * the SCAN_FRAGREQ dynamically each time we send SCAN_FRAGREQ
             * and thus don't have to care what previous SCAN_FRAGREQ's did
             * even though they were part of the same SQL query and even the
             * same pushdown join part.
             *
             * We need to set the query thread flag since this ensures that
             * the query thread use shared access to the fragment.
             */
            jam();
            ref =
                get_scan_fragreq_ref(&c_tc->m_distribution_handle, instance_no);
            fragPtr.p->m_next_ref = ref;
          } else {
            jam();
            /**
             * We are not using query threads in this node, we can set
             * m_next_ref immediately, we can also set m_ref to the proper
             * DBLQH location since there is no flexible scheduling without
             * query threads.
             *
             * There is no need to set the query thread flag here since we
             * already know the location where SCAN_FRAGREQ is executed.
             */
            ref = numberToRef(DBLQH, instance_no, nodeId);
            fragPtr.p->m_ref = ref;
            fragPtr.p->m_next_ref = ref;
          }
        } else {
          Uint32 num_query_threads = getNodeInfo(nodeId).m_query_threads;
          if (num_query_threads > 0) {
            /**
             * The message is sent to a remote node id using query threads.
             * The scheduling of this signal to a receiver will be made by
             * the receiver thread in the receiving node. Thus we don't know
             * which block instance that will execute this signal at this
             * point in time. We set the Query thread flag to ensure that
             * the receiver will return his block reference in subsequent
             * SCAN_FRAGCONF/SCAN_FRAGREF signals.
             *
             * It is ok to send this flag to old nodes as well since they will
             * simply ignore it. It is also ok to send it even if the receiver
             * doesn't use query threads since the only action it will incur is
             * that it will add the reference of the sender in the SCAN_FRAGCONF
             * and SCAN_FRAGREF signals.
             */
            jam();
            Uint32 signal_size = 0;
            for (Uint32 i = 0; i < handle.m_cnt; i++) {
              signal_size += handle.m_ptr[i].sz;
            }
            signal_size += NDB_ARRAY_SIZE(data.m_scanFragReq);
            if (signal_size <= MAX_SIZE_SINGLE_SIGNAL) {
              jam();
              /* Single signals can be sent to virtual blocks. */
              fragPtr.p->m_next_ref = numberToRef(V_QUERY, instance_no, nodeId);
              ScanFragReq::setQueryThreadFlag(req->requestInfo, 1);
            } else {
              jam();
              /**
               * We are about to send a fragmented signal, fragmented signals
               * cannot be handled with virtual blocks when sent to remote
               * nodes since each signal would be independently decided where
               * to send and thus would cause complete confusion.
               *
               * We avoid this by always sending to the LDM thread instance
               * in the LDM group in this particular case.
               */
              ref = numberToRef(DBLQH, instance_no, nodeId);
              fragPtr.p->m_ref = ref;
              fragPtr.p->m_next_ref = ref;
            }
          } else {
            /**
             * The receiver node isn't using query threads, so we simply send
             * it to the normal DBLQH instance.
             */
            jam();
            ref = numberToRef(DBLQH, instance_no, nodeId);
            fragPtr.p->m_ref = ref;
            fragPtr.p->m_next_ref = ref;
          }
        }
      } else {
        /* Here the receiver is known, so no need to set m_next_ref to 0 */
        jam();
        ref = numberToRef(DBLQH, instance_no, nodeId);
        fragPtr.p->m_ref = ref;
        fragPtr.p->m_next_ref = ref;
      }
      if (!ScanFragReq::getRangeScanFlag(req->requestInfo)) {
        jam();
        /**
         * Always TUP scans for full table scans to avoid ACC scans
         * that would create issues with query threads.
         */
        ScanFragReq::setTupScanFlag(req->requestInfo, 1);
      }
      /**
       * To reduce the copy burden we want to keep hold of the
       * AttrInfo and KeyInfo sections after sending them to
       * LQH.  To do this we perform the fragmented send inline,
       * so that all fragments are sent *now*.  This avoids any
       * problems with the fragmented send CONTINUE 'thread' using
       * the section while we hold or even release it.  The
       * signal receiver can still take realtime breaks when
       * receiving.
       *
       * Indicate to sendBatchedFragmentedSignal that we want to
       * keep the fragments, so it must not free them, unless this
       * is the last request in which case they can be freed. If
       * the last request is a local send then a copy is avoided.
       */
      {
        jam();
        sendBatchedFragmentedSignal(ref, GSN_SCAN_FRAGREQ, signal,
                                    NDB_ARRAY_SIZE(data.m_scanFragReq), JBB,
                                    &handle,
                                    !releaseAtSend);  // Keep sent sections,
                                                      // unless last send
      }

      if (releaseAtSend) {
        ndbassert(handle.m_cnt == 0);
      } else if (treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED) {
        // Release the constructed attrInfo
        releaseSection(attrInfoPtrI);
      }
      handle.clear();

      fragPtr.p->m_state = ScanFragHandle::SFH_SCANNING;  // running
      data.m_frags_outstanding++;
      data.m_frags_not_started--;
      data.m_corrIdStart += bs_rows;
      requestsSent++;
      list.next(fragPtr);
    }  // while (requestsSent < noOfFrags)
  }
  if (err) {
    jam();
    abort(signal, requestPtr, err);
  }

  return requestsSent;
}

void Dbspj::scanFrag_parent_batch_repeat(Signal *signal,
                                         Ptr<Request> requestPtr,
                                         Ptr<TreeNode> treeNodePtr) {
  jam();
  ndbassert(treeNodePtr.p->m_parentPtrI != RNIL);

  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;

  DEBUG("scanFrag_parent_batch_repeat(), m_node_no: "
        << treeNodePtr.p->m_node_no
        << ", m_batch_chunks: " << data.m_batch_chunks);

  ndbassert(treeNodePtr.p->m_bits & TreeNode::T_SCAN_REPEATABLE);

  /**
   * Register fragment-scans to be restarted if we didn't get all
   * previously fetched parent related child rows in a single batch.
   */
  if (data.m_batch_chunks > 1) {
    jam();
    DEBUG("Register TreeNode for restart, m_node_no: "
          << treeNodePtr.p->m_node_no);
    ndbrequire(treeNodePtr.p->m_state != TreeNode::TN_ACTIVE);
    registerActiveCursor(requestPtr, treeNodePtr);
    data.m_batch_chunks = 0;

    if (treeNodePtr.p->m_bits & TreeNode::T_REDUCE_KEYS &&
        data.m_rangePtrISave != RNIL) {
      /**
       * We saved the full set of range-keys we had before removeMatchedKeys().
       * Now restore it in preparation for a 'repeat' of the same range scans.
       * Note that only non-pruned scans will removeMatchedKeys().
       */
      jam();

      // The first fragment hold the keys to be requested from all fragments
      Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
      Ptr<ScanFragHandle> firstFragPtr;
      list.first(firstFragPtr);

      if (firstFragPtr.p->m_rangePtrI != RNIL) {
        releaseSection(firstFragPtr.p->m_rangePtrI);
      }
      if (firstFragPtr.p->m_paramPtrI != RNIL) {
        releaseSection(firstFragPtr.p->m_paramPtrI);
      }
      firstFragPtr.p->m_rangePtrI = data.m_rangePtrISave;
      firstFragPtr.p->m_paramPtrI = data.m_paramPtrISave;
      firstFragPtr.p->m_rangeCnt = data.m_rangeCntSave;
      data.m_rangePtrISave = RNIL;
      data.m_paramPtrISave = RNIL;
      data.m_rangeCntSave = 0;
    }
  }
}

/**
 * Remove keys from a prepared SCAN_FRAGREQ where a match was already found in
 * some previous fragment scan(s). Relates to semi-join-firstMatch evaluation
 * of queries, where the matched row value will not be a part of the query
 * result itself, and the query can be concluded when a single match has
 * been found.
 * Relevant queries taking advantage of this are the TPC-H queries Q4, Q21, Q22
 */
void Dbspj::removeMatchedKeys(Ptr<Request> requestPtr,
                              Ptr<TreeNode> treeNodePtr,
                              Ptr<ScanFragHandle> fragPtr) {
  jam();
  ndbassert(treeNodePtr.p->m_scanAncestorPtrI != RNIL);
  Ptr<TreeNode> scanAncestorPtr;
  ndbrequire(m_treenode_pool.getPtr(scanAncestorPtr,
                                    treeNodePtr.p->m_scanAncestorPtrI));
  ndbassert(scanAncestorPtr.p->m_rows.m_type == RowCollection::COLLECTION_MAP);

  RowRef ref;
  scanAncestorPtr.p->m_rows.m_map.copyto(ref);
  const Uint32 *const mapptr = get_row_ptr(ref);

  /**
   * Note that only non-pruned scans will removeMatchedKeys().
   *  -> The first fragment contains all keys to be sent in all REQ's.
   */
  SectionReader rangeInfo(fragPtr.p->m_rangePtrI, getSectionSegmentPool());
  Uint32 newRangePtrI = RNIL;
  Uint32 newRangeCnt = 0;
  Uint32 rangeHead;

  /**
   * There might be a pr-range pushed-condition-parameter as well.
   * There is one such parameter pr range-key.
   */
  SegmentedSectionPtr paramPtr;
  paramPtr.setNull();
  if (fragPtr.p->m_paramPtrI != RNIL) {
    getSection(paramPtr, fragPtr.p->m_paramPtrI);
  }
  SectionReader paramInfo(paramPtr, getSectionSegmentPool());
  Uint32 newParamPtrI = RNIL;

  /* Iterate all key's, skip those having a match in the 'scanAncestor' */
  while (rangeInfo.peekWord(&rangeHead)) {
    const Uint32 length = rangeHead >> 16;
    const Uint32 rowId = (rangeHead & 0xFFF0) >> 4;

    /**
     * We have the rowId of the row this key was constructed from.
     * Relocate the row to check if matches were found for it
     */
    scanAncestorPtr.p->m_rows.m_map.load(mapptr, rowId, ref);
    const Uint32 *const rowptr = get_row_ptr(ref);

    RowPtr row;
    setupRowPtr(scanAncestorPtr, row, rowptr);

    const bool foundMatches = row.m_matched->get(treeNodePtr.p->m_node_no);
    DEBUG("removeMatchedKeys?"
          << ", ancestor:" << scanAncestorPtr.p->m_node_no
          << ", rowId:" << rowId << ", 'matched':" << row.m_matched->rep.data[0]
          << ", foundMatches:" << foundMatches);

    if (foundMatches)  // skip this key range
    {
      jamDebug();
      if (!rangeInfo.step(length)) break;
    } else {
      const Uint32 err = appendReaderToSection(newRangePtrI, rangeInfo, length);
      if (unlikely(err != 0)) {
        /* In case of out of section memory, just keep the existing keys */
        jam();
        releaseSection(newRangePtrI);
        releaseSection(newParamPtrI);
        return;
      }
      newRangeCnt++;
    }

    if (fragPtr.p->m_paramPtrI != RNIL)  // There is a parameter
    {
      Uint32 paramLen;
      paramInfo.peekWord(&paramLen);

      if (foundMatches)  // skip this parameter
      {
        jamDebug();
        if (!paramInfo.step(paramLen)) break;
      } else {
        const Uint32 err =
            appendReaderToSection(newParamPtrI, paramInfo, paramLen);
        if (unlikely(err != 0)) {
          /* In case of out of section memory, just keep the existing parameters
           */
          jam();
          releaseSection(newRangePtrI);
          releaseSection(newParamPtrI);
          return;
        }
      }
    }
  }  // while

  DEBUG("removedMatchedKeys"
        << ", treeNode:" << treeNodePtr.p->m_node_no << ", "
        << fragPtr.p->m_rangeCnt << " -> " << newRangeCnt);

  // As the scan might be 'repeated' we need to save the full key range.
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  if (data.m_rangePtrISave == RNIL) {
    data.m_rangePtrISave = fragPtr.p->m_rangePtrI;
    data.m_rangeCntSave = fragPtr.p->m_rangeCnt;
  } else {
    releaseSection(fragPtr.p->m_rangePtrI);
  }

  // Replace rangeKeys, and parameters if specified.
  fragPtr.p->m_rangePtrI = newRangePtrI;
  fragPtr.p->m_rangeCnt = newRangeCnt;

  if (fragPtr.p->m_paramPtrI != RNIL) {
    if (data.m_paramPtrISave == RNIL) {
      data.m_paramPtrISave = fragPtr.p->m_paramPtrI;
    } else {
      releaseSection(fragPtr.p->m_paramPtrI);
    }
    fragPtr.p->m_paramPtrI = newParamPtrI;
  }
}  // removeMatchedKeys

void Dbspj::scanFrag_countSignal(Signal *signal, Ptr<Request> requestPtr,
                                 Ptr<TreeNode> treeNodePtr, Uint32 cnt) {
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  data.m_rows_received += cnt;

  if (data.m_frags_outstanding == 0 &&
      data.m_rows_received == data.m_rows_expecting) {
    jam();
    ndbassert(requestPtr.p->m_outstanding > 0);
    requestPtr.p->m_outstanding--;

    // We have received all rows for this treeNode in this batch.
    requestPtr.p->m_completed_tree_nodes.set(treeNodePtr.p->m_node_no);
  }
}

void Dbspj::scanFrag_execSCAN_FRAGCONF(Signal *signal, Ptr<Request> requestPtr,
                                       Ptr<TreeNode> treeNodePtr,
                                       Ptr<ScanFragHandle> fragPtr) {
  jam();

  const ScanFragConf *conf = (const ScanFragConf *)(signal->getDataPtr());

  Uint32 rows = conf->completedOps;
  Uint32 done = conf->fragmentCompleted;
  Uint32 bytes = conf->total_len * sizeof(Uint32);

  Uint32 state = fragPtr.p->m_state;
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;

  if (state == ScanFragHandle::SFH_WAIT_CLOSE && done == 0) {
    jam();
    /**
     * We sent an explicit close request...ignore this...a close will come later
     */
    return;
  }
  if (state == ScanFragHandle::SFH_SCANNING_WAIT_CLOSE) {
    if (done == 0) {
      jam();
      /**
       * The request was aborted and we were handling the first SCAN_FRAGREQ.
       * In this case fragPtr.p->m_next_ref was still set to 0, thus we could
       * not send the SCAN_NEXTREQ to close the scan when the request was
       * aborted, we had to wait for the first return signal to arrive.
       *
       * If the return signal is a SCAN_FRAGREF we can simply treat it as if
       * we were in the state SFH_WAIT_CLOSE, if we are done when receiving
       * this message we can also simply treat it as if we already were in
       * the state SFH_WAIT_CLOSE.
       *
       * However when receiving this and not being done, this requires a
       * special close signal to be sent. Now that we have received a
       * SCAN_FRAGCONF we know the exact location of the scan block and are
       * able to send the request now using the updated m_next_ref variable.
       * So we send the close immediately from here.
       *
       * The signal we received here is ignored since the request is already
       * aborted.
       *
       * The reason for this special handling comes from the fact that the
       * receiver can schedule the SCAN_FRAGREQ to many different query
       * threads. DBSPJ only knows this location if the SCAN_FRAGREQ is
       * sent in the same node. If it is sent to another node, the receiving
       * node will perform the scheduling of the signal to its destination
       * query thread or LDM thread.
       */
      send_close_scan(signal, fragPtr, requestPtr);
      return;
    } else {
      jam();
      state = fragPtr.p->m_state = ScanFragHandle::SFH_WAIT_CLOSE;
    }
  }

  requestPtr.p->m_rows += rows;
  fragPtr.p->m_totalRows += rows;
  data.m_totalRows += rows;
  data.m_totalBytes += bytes;

  if (treeNodePtr.p->m_bits & TreeNode::T_EXPECT_TRANSID_AI) {
    jam();
    data.m_rows_expecting += rows;
  }

  ndbrequire(data.m_frags_outstanding);
  ndbrequire(state == ScanFragHandle::SFH_SCANNING ||
             state == ScanFragHandle::SFH_WAIT_CLOSE);

  data.m_frags_outstanding--;
  fragPtr.p->m_state = ScanFragHandle::SFH_WAIT_NEXTREQ;

  if (done) {
    jam();
    fragPtr.p->m_state = ScanFragHandle::SFH_COMPLETE;
    ndbrequire(data.m_frags_complete < data.m_fragCount);
    data.m_frags_complete++;

    // Statistics pr fragments added to total 'data' when fragment completes
    ndbassert(treeNodePtr.p->m_node_no == 0 || fragPtr.p->m_keysSent > 0);
    data.m_completedKeys += fragPtr.p->m_keysSent;
    data.m_completedRows += fragPtr.p->m_totalRows;
    fragPtr.p->m_keysSent = 0;
    fragPtr.p->m_totalRows = 0;

    if (data.m_frags_complete == data.m_fragCount ||
        ((requestPtr.p->m_state & Request::RS_ABORTING) != 0 &&
         data.m_fragCount ==
             (data.m_frags_complete + data.m_frags_not_started))) {
      jam();
      ndbrequire(requestPtr.p->m_cnt_active);
      requestPtr.p->m_cnt_active--;
      treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;
    }
  }

  if (data.m_frags_outstanding == 0) {
    if (data.m_totalRows ==
        0)  // Nothing returned, corrId can start from beginning
    {
      data.m_corrIdStart = 0;
    }

    /**
     * Don't continue scan if we're aborting...
     */
    ndbassert(state != ScanFragHandle::SFH_WAIT_CLOSE ||
              (requestPtr.p->m_state & Request::RS_ABORTING));

    // Collect statistics:
    if (data.m_completedKeys > 0) {
      jam();
      /**
       * Calculate the 'record pr key' fanout for the scan instance
       * that we have just completed, and update 'recsPrKeyStat' with
       * this value. We then use this statistics to calculate
       * the initial parallelism for the next instance of this operation.
       */
      const double recsPrKey =
          double(data.m_completedRows) / data.m_completedKeys;
      data.m_recsPrKeyStat.sample(recsPrKey);
      data.m_completedKeys = 0;
      data.m_completedRows = 0;
    }
    if (data.m_totalRows > 0) {
      const double recSize = double(data.m_totalBytes) / data.m_totalRows;
      data.m_recSizeStat.sample(recSize);
    }

    if (treeNodePtr.p->m_bits & TreeNode::T_REDUCE_KEYS &&
        data.m_frags_not_started > 0) {
      jam();
      /**
       * Reduce ranges needed in later scanFrag_send().
       * Can possibly end entire scan early.
       */
      Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
      Ptr<ScanFragHandle> firstFragPtr;
      list.first(firstFragPtr);
      removeMatchedKeys(requestPtr, treeNodePtr, firstFragPtr);
      data.m_keysToSend = firstFragPtr.p->m_rangeCnt * data.m_frags_not_started;

      if (firstFragPtr.p->m_rangePtrI == RNIL)  // No more range keys -> done
      {
        /**
         * No remaining keys needed to be matched, no need to send
         * the 'm_frags_not_started'. Set these directly to completed.
         */
        jam();
        ndbassert(firstFragPtr.p->m_rangeCnt == 0);
        ndbassert(firstFragPtr.p->m_paramPtrI == RNIL);
        Ptr<ScanFragHandle> nonStartedFragPtr(firstFragPtr);
        while (!nonStartedFragPtr.isNull()) {
          if (nonStartedFragPtr.p->m_state == ScanFragHandle::SFH_NOT_STARTED) {
            jamDebug();
            nonStartedFragPtr.p->m_state = ScanFragHandle::SFH_COMPLETE;
          }
          list.next(nonStartedFragPtr);
        }
        data.m_frags_complete += data.m_frags_not_started;
        data.m_frags_not_started = 0;
        if (data.m_frags_complete == data.m_fragCount) {
          /* All fragments completed -> Done with this treeNode */
          jamDebug();
          ndbassert(requestPtr.p->m_cnt_active > 0);
          requestPtr.p->m_cnt_active--;
          treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;
        }
      }
    }  // T_REDUCE_KEYS

    const bool all_started_completed =
        (data.m_frags_not_started ==
         (data.m_fragCount - data.m_frags_complete));

    if (state != ScanFragHandle::SFH_WAIT_CLOSE &&  // Not closing the scan
        all_started_completed &&                    // All fragments 'done'
        data.m_frags_not_started > 0)               // Pending scanFrag_send()
    {
      jam();
      Uint32 availableBatchRows, availableBatchBytes;
      const Uint32 batchRows = scanFrag_getBatchSize(
          treeNodePtr, availableBatchBytes, availableBatchRows);
      if (batchRows > 0)  // Batch buffer left
      {
        const Uint32 parallelism =
            scanFrag_parallelism(requestPtr, treeNodePtr, batchRows);
        /**
         * Check if it is worthwhile to scan more fragments given the
         * remaining available batch size. We should be able to complete
         * the remaining fragments.
         */
        if (parallelism >= data.m_frags_not_started) {
          jam();
          Uint32 bs_rows = availableBatchRows / parallelism;
          Uint32 bs_bytes = availableBatchBytes / parallelism;
          data.m_parallelism = parallelism;

          DEBUG(
              "::scanFrag_execSCAN_FRAGCONF() batch was not full."
              " Asking for new batches from "
              << parallelism << " fragments with " << bs_rows << " rows and "
              << bs_bytes << " bytes.");

          if (unlikely(bs_rows > bs_bytes)) bs_rows = bs_bytes;

          const Uint32 frags_started = scanFrag_send(
              signal, requestPtr, treeNodePtr, parallelism, bs_bytes, bs_rows);

          if (likely(frags_started > 0)) return;

          // Else: scanFrag_send() didn't send anything for some reason.
          // Need to continue into 'completion detection' below.
          jam();
        }
      }
    }

    if (data.m_rows_received == data.m_rows_expecting ||
        state == ScanFragHandle::SFH_WAIT_CLOSE) {
      jam();
      ndbassert(requestPtr.p->m_outstanding > 0);
      requestPtr.p->m_outstanding--;
      requestPtr.p->m_completed_tree_nodes.set(treeNodePtr.p->m_node_no);
      handleTreeNodeComplete(signal, requestPtr, treeNodePtr);
    }
  }  // if (data.m_frags_outstanding == 0)
}

void Dbspj::scanFrag_execSCAN_FRAGREF(Signal *signal, Ptr<Request> requestPtr,
                                      Ptr<TreeNode> treeNodePtr,
                                      Ptr<ScanFragHandle> fragPtr) {
  jam();

  const ScanFragRef *rep = CAST_CONSTPTR(ScanFragRef, signal->getDataPtr());
  const Uint32 errCode = rep->errorCode;

  Uint32 state = fragPtr.p->m_state;
  ndbrequire(state == ScanFragHandle::SFH_SCANNING ||
             state == ScanFragHandle::SFH_SCANNING_WAIT_CLOSE ||
             state == ScanFragHandle::SFH_WAIT_CLOSE);

  fragPtr.p->m_state = ScanFragHandle::SFH_COMPLETE;

  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  ndbrequire(data.m_frags_complete < data.m_fragCount);
  data.m_frags_complete++;
  ndbrequire(data.m_frags_outstanding > 0);
  data.m_frags_outstanding--;

  if (data.m_fragCount == (data.m_frags_complete + data.m_frags_not_started)) {
    jam();
    ndbrequire(requestPtr.p->m_cnt_active);
    requestPtr.p->m_cnt_active--;
    treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;
  }

  if (data.m_frags_outstanding == 0) {
    jam();
    ndbrequire(requestPtr.p->m_outstanding);
    requestPtr.p->m_outstanding--;
  }

  abort(signal, requestPtr, errCode);
}

void Dbspj::scanFrag_execSCAN_NEXTREQ(Signal *signal, Ptr<Request> requestPtr,
                                      Ptr<TreeNode> treeNodePtr) {
  jam();
  Uint32 err = checkTableError(treeNodePtr);
  if (unlikely(err)) {
    jam();
    abort(signal, requestPtr, err);
    return;
  }

  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  const ScanFragReq *org = reinterpret_cast<ScanFragReq *>(data.m_scanFragReq);
  ndbassert(data.m_frags_outstanding == 0);

  data.m_corrIdStart = 0;
  data.m_totalRows = 0;
  data.m_totalBytes = 0;
  data.m_rows_received = 0;
  data.m_rows_expecting = 0;

  if (data.m_frags_not_started == 0) {
    // Just complete those already ongoing
    jam();
    data.m_parallelism = data.m_fragCount - data.m_frags_complete;
  } else {
    jam();
    Uint32 availableBatchRows, availableBatchBytes;
    const Uint32 batchRows = scanFrag_getBatchSize(
        treeNodePtr, availableBatchBytes, availableBatchRows);

    data.m_parallelism =
        scanFrag_parallelism(requestPtr, treeNodePtr, batchRows);

#ifdef DEBUG_SCAN_FRAGREQ
    DEBUG("::scanFrag_execSCAN_NEXTREQ() Asking for new batches from "
          << data.m_parallelism << " fragments with "
          << availableBatchRows / data.m_parallelism << " rows and "
          << availableBatchBytes / data.m_parallelism << " bytes.");
#endif
  }

  Uint32 bs_rows =
      MIN(org->batch_size_rows / data.m_parallelism, MAX_PARALLEL_OP_PER_SCAN);
  const Uint32 bs_bytes = org->batch_size_bytes / data.m_parallelism;
  ndbassert(bs_rows > 0);

  /**
   * A SORTED_ORDER scan need to fetch one row at a time from the treeNode
   * to be ordered - See reasoning where we set the T_SORTED_ORDER bit.
   */
  if (treeNodePtr.p->m_bits & TreeNode::T_SORTED_ORDER &&
      requestPtr.p->m_bits & Request::RT_MULTI_SCAN) {
    bs_rows = 1;
  }

  const Uint32 sentFragCount = scanFrag_send_NEXTREQ(
      signal, requestPtr, treeNodePtr, data.m_parallelism, bs_bytes, bs_rows);

  Uint32 frags_started = 0;
  if (sentFragCount < data.m_parallelism) {
    /**
     * Then start new fragments until we reach data.m_parallelism.
     */
    jam();
    ndbassert(data.m_frags_not_started > 0);
    frags_started =
        scanFrag_send(signal, requestPtr, treeNodePtr,
                      data.m_parallelism - sentFragCount, bs_bytes, bs_rows);
  }

  /**
   * sendSignal() or scanFrag_send() might have failed to send:
   * Check that we really did send something before
   * updating outstanding & active.
   */
  if (likely(sentFragCount + frags_started > 0)) {
    jam();
    ndbrequire(data.m_batch_chunks > 0);
    data.m_batch_chunks++;

    requestPtr.p->m_outstanding++;
    requestPtr.p->m_completed_tree_nodes.clear(treeNodePtr.p->m_node_no);
    ndbassert(treeNodePtr.p->m_state == TreeNode::TN_ACTIVE);
  }
}

/**
 * send SCAN_NEXTREQs to the number of fragments specified in
 * noOfFrags. Return number of REQs actually sent.
 */
Uint32 Dbspj::scanFrag_send_NEXTREQ(Signal *signal, Ptr<Request> requestPtr,
                                    Ptr<TreeNode> treeNodePtr, Uint32 noOfFrags,
                                    Uint32 bs_bytes, Uint32 bs_rows) {
  jam();
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  ScanFragNextReq *req =
      reinterpret_cast<ScanFragNextReq *>(signal->getDataPtrSend());
  req->requestInfo = 0;
  ScanFragNextReq::setCorrFactorFlag(req->requestInfo);
  req->transId1 = requestPtr.p->m_transId[0];
  req->transId2 = requestPtr.p->m_transId[1];
  req->batch_size_rows = bs_rows;
  req->batch_size_bytes = bs_bytes;

  Ptr<ScanFragHandle> fragPtr;
  Uint32 sentFragCount = 0;
  {
    /**
     * First, ask for more data from fragments that are already started.
     */
    Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
    list.first(fragPtr);
    while (sentFragCount < noOfFrags && !fragPtr.isNull()) {
      jam();
      ndbassert(fragPtr.p->m_state == ScanFragHandle::SFH_WAIT_NEXTREQ ||
                fragPtr.p->m_state == ScanFragHandle::SFH_COMPLETE ||
                fragPtr.p->m_state == ScanFragHandle::SFH_NOT_STARTED);
      if (fragPtr.p->m_state == ScanFragHandle::SFH_WAIT_NEXTREQ) {
        jam();

        req->variableData[0] = data.m_corrIdStart;
        fragPtr.p->m_state = ScanFragHandle::SFH_SCANNING;
        data.m_corrIdStart += bs_rows;
        data.m_frags_outstanding++;

        DEBUG("scanFrag_send_NEXTREQ to: "
              << hex << fragPtr.p->m_ref
              << ", m_node_no=" << treeNodePtr.p->m_node_no
              << ", senderData: " << req->senderData);

#ifdef DEBUG_SCAN_FRAGREQ
        printSCANFRAGNEXTREQ(stdout, &signal->theData[0],
                             ScanFragNextReq::SignalLength + 1, DBLQH);
#endif

        req->senderData = fragPtr.i;
        ndbrequire(refToMain(fragPtr.p->m_next_ref) != V_QUERY);
        sendSignal(fragPtr.p->m_next_ref, GSN_SCAN_NEXTREQ, signal,
                   ScanFragNextReq::SignalLength + 1, JBB);
        sentFragCount++;
      }
      list.next(fragPtr);
    }
  }
  return sentFragCount;
}

void Dbspj::scanFrag_complete(Signal *signal, Ptr<Request> requestPtr,
                              Ptr<TreeNode> treeNodePtr) {
  jam();
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  if (!data.m_fragments.isEmpty()) {
    jam();
    DihScanTabCompleteRep *rep =
        (DihScanTabCompleteRep *)signal->getDataPtrSend();
    rep->tableId = treeNodePtr.p->m_tableOrIndexId;
    rep->scanCookie = data.m_scanCookie;
    rep->jamBufferPtr = jamBuffer();

    EXECUTE_DIRECT_MT(DBDIH, GSN_DIH_SCAN_TAB_COMPLETE_REP, signal,
                      DihScanTabCompleteRep::SignalLength, 0);
  }
}

void Dbspj::scanFrag_abort(Signal *signal, Ptr<Request> requestPtr,
                           Ptr<TreeNode> treeNodePtr) {
  jam();
  // Correlation ids for deferred operations are allocated in the batch specific
  // arena. It is sufficient to release entire memory arena.
  m_arenaAllocator.release(treeNodePtr.p->m_batchArena);
  treeNodePtr.p->m_deferred.init();

  switch (treeNodePtr.p->m_state) {
    case TreeNode::TN_BUILDING:
    case TreeNode::TN_PREPARING:
    case TreeNode::TN_INACTIVE:
    case TreeNode::TN_COMPLETING:
    case TreeNode::TN_END:
      DEBUG("scanFrag_abort"
            << ", transId: " << hex << requestPtr.p->m_transId[0] << "," << hex
            << requestPtr.p->m_transId[1]
            << ", state: " << treeNodePtr.p->m_state);
      return;

    case TreeNode::TN_ACTIVE:
      jam();
      break;
  }

  ScanFragNextReq *req = CAST_PTR(ScanFragNextReq, signal->getDataPtrSend());
  req->requestInfo = 0;
  ScanFragNextReq::setCloseFlag(req->requestInfo, 1);
  req->transId1 = requestPtr.p->m_transId[0];
  req->transId2 = requestPtr.p->m_transId[1];
  req->batch_size_rows = 0;
  req->batch_size_bytes = 0;

  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
  Ptr<ScanFragHandle> fragPtr;

  Uint32 cnt_waiting = 0;
  Uint32 cnt_scanning = 0;
  for (list.first(fragPtr); !fragPtr.isNull(); list.next(fragPtr)) {
    switch (fragPtr.p->m_state) {
      case ScanFragHandle::SFH_SCANNING_WAIT_CLOSE:
        ndbabort();
        break;
      case ScanFragHandle::SFH_NOT_STARTED:
      case ScanFragHandle::SFH_COMPLETE:
      case ScanFragHandle::SFH_WAIT_CLOSE:
        jam();
        break;
      case ScanFragHandle::SFH_WAIT_NEXTREQ:
        jam();
        cnt_waiting++;               // was idle...
        data.m_frags_outstanding++;  // is closing
        goto do_abort;
      case ScanFragHandle::SFH_SCANNING:
        jam();
        cnt_scanning++;
        goto do_abort;
      do_abort:
        req->senderData = fragPtr.i;
        if (refToMain(fragPtr.p->m_next_ref) != V_QUERY) {
          jam();
          sendSignal(fragPtr.p->m_next_ref, GSN_SCAN_NEXTREQ, signal,
                     ScanFragNextReq::SignalLength, JBB);
          fragPtr.p->m_state = ScanFragHandle::SFH_WAIT_CLOSE;
        } else {
          jam();
          /**
           * We don't know where the SCAN_FRAGREQ is executing yet. We will
           * be informed of this when the SCAN_FRAGCONF is received.
           * We will handle the close sending when this signal is received.
           */
          ndbrequire(fragPtr.p->m_state == ScanFragHandle::SFH_SCANNING);
          fragPtr.p->m_state = ScanFragHandle::SFH_SCANNING_WAIT_CLOSE;
        }
        break;
    }
  }

  if (cnt_scanning == 0) {
    if (cnt_waiting > 0) {
      /**
       * If all were waiting...this should increase m_outstanding
       */
      jam();
      requestPtr.p->m_outstanding++;
    } else {
      /**
       * All fragments are either complete or not yet started, so there is
       * nothing to abort.
       */
      jam();
      ndbassert(data.m_frags_not_started > 0);
      ndbrequire(requestPtr.p->m_cnt_active);
      requestPtr.p->m_cnt_active--;
      treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;
    }
  }
}

Uint32 Dbspj::scanFrag_execNODE_FAILREP(Signal *signal, Ptr<Request> requestPtr,
                                        Ptr<TreeNode> treeNodePtr,
                                        const NdbNodeBitmask nodes) {
  jam();

  switch (treeNodePtr.p->m_state) {
    case TreeNode::TN_PREPARING:
    case TreeNode::TN_INACTIVE:
      return 1;

    case TreeNode::TN_BUILDING:
    case TreeNode::TN_COMPLETING:
    case TreeNode::TN_END:
      return 0;

    case TreeNode::TN_ACTIVE:
      jam();
      break;
  }

  Uint32 sum = 0;
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
  Ptr<ScanFragHandle> fragPtr;

  Uint32 save0 = data.m_frags_outstanding;
  Uint32 save1 = data.m_frags_complete;

  for (list.first(fragPtr); !fragPtr.isNull(); list.next(fragPtr)) {
    if (nodes.get(refToNode(fragPtr.p->m_ref)) == false) {
      jam();
      /**
       * No action needed
       */
      continue;
    }

    switch (fragPtr.p->m_state) {
      case ScanFragHandle::SFH_NOT_STARTED:
        jam();
        ndbrequire(data.m_frags_complete < data.m_fragCount);
        data.m_frags_complete++;
        ndbrequire(data.m_frags_not_started > 0);
        data.m_frags_not_started--;
        [[fallthrough]];
      case ScanFragHandle::SFH_COMPLETE:
        jam();
        sum++;  // indicate that we should abort
        /**
         * we could keep list of all fragments...
         *   or execute DIGETNODES again...
         *   but for now, we don't
         */
        break;
      case ScanFragHandle::SFH_WAIT_CLOSE:
      case ScanFragHandle::SFH_SCANNING:
      case ScanFragHandle::SFH_SCANNING_WAIT_CLOSE:
        jam();
        ndbrequire(data.m_frags_outstanding > 0);
        data.m_frags_outstanding--;
        [[fallthrough]];
      case ScanFragHandle::SFH_WAIT_NEXTREQ:
        jam();
        sum++;
        ndbrequire(data.m_frags_complete < data.m_fragCount);
        data.m_frags_complete++;
        break;
    }
    fragPtr.p->m_ref = 0;
    fragPtr.p->m_next_ref = 0;
    fragPtr.p->m_state = ScanFragHandle::SFH_COMPLETE;
  }

  if (save0 != 0 && data.m_frags_outstanding == 0) {
    jam();
    ndbrequire(requestPtr.p->m_outstanding);
    requestPtr.p->m_outstanding--;
  }

  if (save1 != 0 &&
      data.m_fragCount == (data.m_frags_complete + data.m_frags_not_started)) {
    jam();
    ndbrequire(requestPtr.p->m_cnt_active);
    requestPtr.p->m_cnt_active--;
    treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;
  }

  return sum;
}

void Dbspj::scanFrag_release_rangekeys(Ptr<Request> requestPtr,
                                       Ptr<TreeNode> treeNodePtr) {
  jam();
  DEBUG("scanFrag_release_rangekeys(), tree node "
        << treeNodePtr.i << " m_node_no: " << treeNodePtr.p->m_node_no);

  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
  Ptr<ScanFragHandle> fragPtr;

  if (treeNodePtr.p->m_bits & TreeNode::T_PRUNE_PATTERN) {
    jam();
    for (list.first(fragPtr); !fragPtr.isNull(); list.next(fragPtr)) {
      if (fragPtr.p->m_rangePtrI != RNIL) {
        releaseSection(fragPtr.p->m_rangePtrI);
        fragPtr.p->m_rangePtrI = RNIL;
        fragPtr.p->m_rangeCnt = 0;
      }
      if (fragPtr.p->m_paramPtrI != RNIL) {
        releaseSection(fragPtr.p->m_paramPtrI);
        fragPtr.p->m_paramPtrI = RNIL;
      }
    }
  } else {
    /**
     * Range scan is not 'pruned' -> the first fragment(only) hold
     * the keys to be used for all fragments.
     */
    jam();
    if (!list.first(fragPtr)) return;
    if (fragPtr.p->m_rangePtrI != RNIL) {
      releaseSection(fragPtr.p->m_rangePtrI);
      fragPtr.p->m_rangePtrI = RNIL;
      fragPtr.p->m_rangeCnt = 0;
    }
    if (fragPtr.p->m_paramPtrI != RNIL) {
      releaseSection(fragPtr.p->m_paramPtrI);
      fragPtr.p->m_paramPtrI = RNIL;
    }
  }

  if (data.m_rangePtrISave != RNIL) {
    releaseSection(data.m_rangePtrISave);
  }
  if (data.m_paramPtrISave != RNIL) {
    releaseSection(data.m_paramPtrISave);
  }
  data.m_rangePtrISave = RNIL;
  data.m_paramPtrISave = RNIL;
  data.m_rangeCntSave = 0;
}

/**
 * Parent batch has completed, and will not refetch (X-joined) results
 * from its children. Release & reset range keys and parameters which are
 * unsent or we have kept for possible resubmits.
 */
void Dbspj::scanFrag_parent_batch_cleanup(Ptr<Request> requestPtr,
                                          Ptr<TreeNode> treeNodePtr,
                                          bool done) {
  DEBUG("scanFrag_parent_batch_cleanup");
  scanFrag_release_rangekeys(requestPtr, treeNodePtr);

  if (done) {
    // Reset client batch state counters
    ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
    data.m_corrIdStart = 0;
    data.m_totalRows = 0;
    data.m_totalBytes = 0;
  }
}

/**
 * Do final cleanup of specified TreeNode. There will be no
 * more (re-)execution of either this TreeNode nor other,
 * so no need to re-init for further execution.
 */
void Dbspj::scanFrag_cleanup(Ptr<Request> requestPtr,
                             Ptr<TreeNode> treeNodePtr) {
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
  DEBUG("scanFrag_cleanup");

  /**
   * Range keys has been collected wherever there are uncompleted
   * parent batches...release them to avoid memleak.
   */
  scanFrag_release_rangekeys(requestPtr, treeNodePtr);

  /**
   * Disallow referring the fragPtr memory object from incoming signals.
   */
  {
    Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
    Ptr<ScanFragHandle> fragPtr;
    for (list.first(fragPtr); !fragPtr.isNull(); list.next(fragPtr)) {
      removeGuardedPtr(fragPtr);
    }
  }

  if (treeNodePtr.p->m_bits & TreeNode::T_PRUNE_PATTERN) {
    jam();
    LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                               m_dependency_map_pool);
    Local_pattern_store pattern(pool, data.m_prunePattern);
    pattern.release();
  } else if (treeNodePtr.p->m_bits & TreeNode::T_CONST_PRUNE) {
    jam();
    if (data.m_constPrunePtrI != RNIL) {
      jam();
      releaseSection(data.m_constPrunePtrI);
      data.m_constPrunePtrI = RNIL;
    }
  }

  cleanup_common(requestPtr, treeNodePtr);
}

bool Dbspj::scanFrag_checkNode(const Ptr<Request> requestPtr,
                               const Ptr<TreeNode> treeNodePtr) {
  jam();
  if (treeNodePtr.p->m_state != TreeNode::TN_ACTIVE) {
    return true;
  }

  bool checkResult = true;

  {
    ScanFragData &data = treeNodePtr.p->m_scanFrag_data;
    Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
    Ptr<ScanFragHandle> fragPtr;

    Uint32 frags_not_started = 0;
    Uint32 frags_outstanding_scan = 0;
    Uint32 frags_outstanding_close = 0;
    Uint32 frags_waiting = 0;
    Uint32 frags_completed = 0;

    Uint32 fragCount = 0;

    for (list.first(fragPtr); !fragPtr.isNull(); list.next(fragPtr)) {
      fragCount++;
      switch (fragPtr.p->m_state) {
        case ScanFragHandle::SFH_NOT_STARTED:
          jam();
          frags_not_started++;
          break;
        case ScanFragHandle::SFH_SCANNING:
          jam();
          frags_outstanding_scan++;
          break;
        case ScanFragHandle::SFH_WAIT_CLOSE:
        case ScanFragHandle::SFH_SCANNING_WAIT_CLOSE:
          jam();
          frags_outstanding_close++;
          break;
        case ScanFragHandle::SFH_WAIT_NEXTREQ:
          jam();
          frags_waiting++;
          break;
        case ScanFragHandle::SFH_COMPLETE:
          jam();
          frags_completed++;
          break;
        default:
          checkResult &= spjCheck(false);
          break;
      }
    }

    /**
     * Compare counters to state, state must be valid
     * at all stable points in time for execNODE_FAILREP
     * handling
     */
    checkResult &= spjCheck(data.m_frags_not_started == frags_not_started);
    checkResult &= spjCheck(data.m_frags_outstanding ==
                            (frags_outstanding_scan + frags_outstanding_close));
    checkResult &= spjCheck(data.m_frags_complete == frags_completed);
  }

  return checkResult;
}

void Dbspj::scanFrag_dumpNode(const Ptr<Request> requestPtr,
                              const Ptr<TreeNode> treeNodePtr) {
  jam();

  /* Non const ref due to list iteration below */
  ScanFragData &data = treeNodePtr.p->m_scanFrag_data;

  g_eventLogger->info(
      "DBSPJ %u :       ScanFrag fragCount %u frags_complete %u "
      "frags_outstanding %u frags_not_started %u ",
      instance(), data.m_fragCount, data.m_frags_complete,
      data.m_frags_outstanding, data.m_frags_not_started);
  g_eventLogger->info(
      "DBSPJ %u :       parallelism %u rows_expecting %u "
      "rows_received %u",
      instance(), data.m_parallelism, data.m_rows_expecting,
      data.m_rows_received);
  g_eventLogger->info(
      "DBSPJ %u :       totalRows %u totalBytes %u "
      "constPrunePtrI %u",
      instance(), data.m_totalRows, data.m_totalBytes, data.m_constPrunePtrI);
  {
    Local_ScanFragHandle_list list(m_scanfraghandle_pool, data.m_fragments);
    Ptr<ScanFragHandle> fragPtr;
    for (list.first(fragPtr); !fragPtr.isNull(); list.next(fragPtr)) {
      dumpScanFragHandle(fragPtr);
    }
  }
}

/**
 * END - MODULE SCAN FRAGMENT
 */

/**
 * Static OpInfo handling
 */
const Dbspj::OpInfo *Dbspj::getOpInfo(Uint32 op) {
  DEBUG("getOpInfo(" << op << ")");
  switch (op) {
    case QueryNode::QN_LOOKUP:
      return &Dbspj::g_LookupOpInfo;
    case QueryNode::QN_SCAN_FRAG_v1:
      return NULL;  // Deprecated, converted into QN_SCAN_FRAG
    case QueryNode::QN_SCAN_INDEX_v1:
      return NULL;  // Deprecated, converted into QN_SCAN_FRAG
    case QueryNode::QN_SCAN_FRAG:
      return &Dbspj::g_ScanFragOpInfo;
    default:
      return 0;
  }
}

/**
 * MODULE COMMON PARSE/UNPACK
 */

/**
 *  @returns dstLen + 1 on error
 */
static Uint32 unpackList(Uint32 dstLen, Uint32 *dst, Dbspj::DABuffer &buffer) {
  const Uint32 *ptr = buffer.ptr;
  if (likely(ptr != buffer.end)) {
    Uint32 tmp = *ptr++;
    Uint32 cnt = tmp & 0xFFFF;

    *dst++ = (tmp >> 16);  // Store first
    DEBUG("cnt: " << cnt << " first: " << (tmp >> 16));

    if (cnt > 1) {
      Uint32 len = cnt / 2;
      if (unlikely(cnt >= dstLen || (ptr + len > buffer.end))) goto error;

      cnt--;  // subtract item stored in header

      for (Uint32 i = 0; i < cnt / 2; i++) {
        *dst++ = (*ptr) & 0xFFFF;
        *dst++ = (*ptr) >> 16;
        ptr++;
      }

      if (cnt & 1) {
        *dst++ = *ptr & 0xFFFF;
        ptr++;
      }

      cnt++;  // re-add item stored in header
    }
    buffer.ptr = ptr;
    return cnt;
  }
  return 0;

error:
  return dstLen + 1;
}

/**
 * This function takes an array of attrinfo, and builds "header"
 *   which can be used to do random access inside the row
 */
Uint32 Dbspj::buildRowHeader(RowPtr::Header *header, LinearSectionPtr ptr) {
  const Uint32 *src = ptr.p;
  const Uint32 len = ptr.sz;
  Uint32 *dst = header->m_offset;
  const Uint32 *const save = dst;
  Uint32 offset = 0;
  do {
    *dst++ = offset;
    const Uint32 tmp = *src++;
    const Uint32 tmp_len = AttributeHeader::getDataSize(tmp);
    offset += 1 + tmp_len;
    src += tmp_len;
  } while (offset < len);

  return header->m_len = static_cast<Uint32>(dst - save);
}

/**
 * This function takes an array of attrinfo, and builds "header"
 *   which can be used to do random access inside the row
 */
Uint32 Dbspj::buildRowHeader(RowPtr::Header *header, const Uint32 *&src,
                             Uint32 cnt) {
  const Uint32 *_src = src;
  Uint32 *dst = header->m_offset;
  const Uint32 *save = dst;
  Uint32 offset = 0;
  for (Uint32 i = 0; i < cnt; i++) {
    *dst++ = offset;
    Uint32 tmp = *_src++;
    Uint32 tmp_len = AttributeHeader::getDataSize(tmp);
    offset += 1 + tmp_len;
    _src += tmp_len;
  }
  src = _src;
  return header->m_len = static_cast<Uint32>(dst - save);
}

Uint32 Dbspj::appendToPattern(Local_pattern_store &pattern, DABuffer &tree,
                              Uint32 len) {
  jam();
  if (unlikely(tree.ptr + len > tree.end))
    return DbspjErr::InvalidTreeNodeSpecification;

  if (ERROR_INSERTED_CLEAR(17008)) {
    g_eventLogger->info(
        "Injecting OutOfQueryMemory error 17008 at line %d file %s", __LINE__,
        __FILE__);
    jam();
    return DbspjErr::OutOfQueryMemory;
  }
  if (unlikely(pattern.append(tree.ptr, len) == 0))
    return DbspjErr::OutOfQueryMemory;

  tree.ptr += len;
  return 0;
}

Uint32 Dbspj::appendParamToPattern(Local_pattern_store &dst,
                                   const RowPtr::Row &row, Uint32 col) {
  jam();
  Uint32 offset = row.m_header->m_offset[col];
  const Uint32 *ptr = row.m_data + offset;
  Uint32 len = AttributeHeader::getDataSize(*ptr++);
  /* Param COL's converted to DATA when appended to pattern */
  Uint32 info = QueryPattern::data(len);

  if (ERROR_INSERTED_CLEAR(17009)) {
    g_eventLogger->info(
        "Injecting OutOfQueryMemory error 17009 at line %d file %s", __LINE__,
        __FILE__);
    jam();
    return DbspjErr::OutOfQueryMemory;
  }

  return dst.append(&info, 1) && dst.append(ptr, len)
             ? 0
             : DbspjErr::OutOfQueryMemory;
}

#ifdef ERROR_INSERT
static int fi_cnt = 0;
bool Dbspj::appendToSection(Uint32 &firstSegmentIVal, const Uint32 *src,
                            Uint32 len) {
  if (ERROR_INSERTED(17510) && fi_cnt++ % 13 == 0) {
    jam();
    g_eventLogger->info(
        "Injecting appendToSection error 17510 at line %d file %s", __LINE__,
        __FILE__);
    return false;
  } else {
    return SimulatedBlock::appendToSection(firstSegmentIVal, src, len);
  }
}
#endif

Uint32 Dbspj::appendParamHeadToPattern(Local_pattern_store &dst,
                                       const RowPtr::Row &row, Uint32 col) {
  jam();
  Uint32 offset = row.m_header->m_offset[col];
  const Uint32 *ptr = row.m_data + offset;
  Uint32 len = AttributeHeader::getDataSize(*ptr);
  /* Param COL's converted to DATA when appended to pattern */
  Uint32 info = QueryPattern::data(len + 1);

  if (ERROR_INSERTED_CLEAR(17010)) {
    g_eventLogger->info(
        "Injecting OutOfQueryMemory error 17010 at line %d file %s", __LINE__,
        __FILE__);
    jam();
    return DbspjErr::OutOfQueryMemory;
  }

  return dst.append(&info, 1) && dst.append(ptr, len + 1)
             ? 0
             : DbspjErr::OutOfQueryMemory;
}

Uint32 Dbspj::appendReaderToSection(Uint32 &ptrI, SectionReader &reader,
                                    Uint32 len) {
  while (len > 0) {
    jam();
    const Uint32 *readPtr;
    Uint32 readLen;
    ndbrequire(reader.getWordsPtr(len, readPtr, readLen));
    if (unlikely(!appendToSection(ptrI, readPtr, readLen)))
      return DbspjErr::OutOfSectionMemory;
    len -= readLen;
  }
  return 0;
}

void Dbspj::getCorrelationData(const RowPtr::Row &row, Uint32 col,
                               Uint32 &correlationNumber) {
  /**
   * TODO handle errors
   */
  Uint32 offset = row.m_header->m_offset[col];
  Uint32 tmp = row.m_data[offset];
  Uint32 len = AttributeHeader::getDataSize(tmp);
  ndbrequire(len == 1);
  ndbrequire(AttributeHeader::getAttributeId(tmp) ==
             AttributeHeader::CORR_FACTOR32);
  correlationNumber = row.m_data[offset + 1];
}

Uint32 Dbspj::appendColToSection(Uint32 &dst, const RowPtr::Row &row,
                                 Uint32 col, bool &hasNull) {
  jam();
  Uint32 offset = row.m_header->m_offset[col];
  const Uint32 *ptr = row.m_data + offset;
  Uint32 len = AttributeHeader::getDataSize(*ptr++);
  if (unlikely(len == 0)) {
    jam();
    hasNull = true;  // NULL-value in key
    return 0;
  }
  return appendToSection(dst, ptr, len) ? 0 : DbspjErr::OutOfSectionMemory;
}

Uint32 Dbspj::appendAttrinfoToSection(Uint32 &dst, const RowPtr::Row &row,
                                      Uint32 col, bool &hasNull) {
  jam();
  Uint32 offset = row.m_header->m_offset[col];
  const Uint32 *ptr = row.m_data + offset;
  Uint32 len = AttributeHeader::getDataSize(*ptr);
  if (unlikely(len == 0)) {
    jam();
    hasNull = true;  // NULL-value in key
  }
  return appendToSection(dst, ptr, 1 + len) ? 0 : DbspjErr::OutOfSectionMemory;
}

/**
 * 'PkCol' is the composite NDB$PK column in an unique index consisting of
 * a fragment id and the composite PK value (all PK columns concatenated)
 */
Uint32 Dbspj::appendPkColToSection(Uint32 &dst, const RowPtr::Row &row,
                                   Uint32 col) {
  jam();
  Uint32 offset = row.m_header->m_offset[col];
  Uint32 tmp = row.m_data[offset];
  Uint32 len = AttributeHeader::getDataSize(tmp);
  ndbrequire(len > 1);  // NULL-value in PkKey is an error
  return appendToSection(dst, row.m_data + offset + 2, len - 1)
             ? 0
             : DbspjErr::OutOfSectionMemory;
}

Uint32 Dbspj::appendFromParent(Uint32 &dst, Local_pattern_store &pattern,
                               Local_pattern_store::ConstDataBufferIterator &it,
                               Uint32 levels, const RowPtr &rowptr,
                               bool &hasNull) {
  jam();
  Ptr<TreeNode> treeNodePtr;
  ndbrequire(m_treenode_pool.getPtr(treeNodePtr, rowptr.m_src_node_ptrI));
  Uint32 corrVal = rowptr.m_src_correlation;
  RowPtr targetRow;
  DEBUG("appendFromParent-of"
        << " node: " << treeNodePtr.p->m_node_no);
  while (levels--) {
    jam();
    if (unlikely(treeNodePtr.p->m_parentPtrI == RNIL)) {
      DEBUG_CRASH();
      return DbspjErr::InvalidPattern;
    }
    ndbrequire(
        m_treenode_pool.getPtr(treeNodePtr, treeNodePtr.p->m_parentPtrI));
    DEBUG("appendFromParent"
          << ", node: " << treeNodePtr.p->m_node_no);
    if (unlikely(treeNodePtr.p->m_rows.m_type !=
                 RowCollection::COLLECTION_MAP)) {
      DEBUG_CRASH();
      return DbspjErr::InvalidPattern;
    }

    RowRef ref;
    treeNodePtr.p->m_rows.m_map.copyto(ref);
    const Uint32 *const mapptr = get_row_ptr(ref);

    Uint32 pos = corrVal >> 16;  // parent corr-val
    if (unlikely(!(pos < treeNodePtr.p->m_rows.m_map.m_size))) {
      DEBUG_CRASH();
      return DbspjErr::InvalidPattern;
    }

    // load ref to parent row
    treeNodePtr.p->m_rows.m_map.load(mapptr, pos, ref);

    const Uint32 *const rowptr = get_row_ptr(ref);
    setupRowPtr(treeNodePtr, targetRow, rowptr);

    if (levels) {
      jam();
      getCorrelationData(targetRow.m_row_data,
                         targetRow.m_row_data.m_header->m_len - 1, corrVal);
    }
  }

  if (unlikely(it.isNull())) {
    DEBUG_CRASH();
    return DbspjErr::InvalidPattern;
  }

  Uint32 info = *it.data;
  Uint32 type = QueryPattern::getType(info);
  Uint32 val = QueryPattern::getLength(info);
  pattern.next(it);
  switch (type) {
    case QueryPattern::P_COL:
      jam();
      return appendColToSection(dst, targetRow.m_row_data, val, hasNull);
    case QueryPattern::P_UNQ_PK:
      jam();
      return appendPkColToSection(dst, targetRow.m_row_data, val);
    case QueryPattern::P_ATTRINFO:
      jam();
      return appendAttrinfoToSection(dst, targetRow.m_row_data, val, hasNull);
    case QueryPattern::P_DATA:
      jam();
      // retrieving DATA from parent...is...an error
      DEBUG_CRASH();
      return DbspjErr::InvalidPattern;
    case QueryPattern::P_PARENT:
      jam();
      // no point in nesting P_PARENT...an error
      DEBUG_CRASH();
      return DbspjErr::InvalidPattern;
    case QueryPattern::P_PARAM:
    case QueryPattern::P_PARAM_HEADER:
      jam();
      // should have been expanded during build
      DEBUG_CRASH();
      return DbspjErr::InvalidPattern;
    default:
      jam();
      DEBUG_CRASH();
      return DbspjErr::InvalidPattern;
  }
}

Uint32 Dbspj::appendDataToSection(
    Uint32 &ptrI, Local_pattern_store &pattern,
    Local_pattern_store::ConstDataBufferIterator &it, Uint32 len,
    bool &hasNull) {
  jam();
  if (unlikely(len == 0)) {
    jam();
    hasNull = true;
    return 0;
  }

#if 0
  /**
   * TODO handle errors
   */
  Uint32 tmp[NDB_SECTION_SEGMENT_SZ];
  while (len > NDB_SECTION_SEGMENT_SZ)
  {
    pattern.copyout(tmp, NDB_SECTION_SEGMENT_SZ, it);
    appendToSection(ptrI, tmp, NDB_SECTION_SEGMENT_SZ);
    len -= NDB_SECTION_SEGMENT_SZ;
  }

  pattern.copyout(tmp, len, it);
  appendToSection(ptrI, tmp, len);
  return 0;
#else
  Uint32 remaining = len;
  Uint32 dstIdx = 0;
  Uint32 tmp[NDB_SECTION_SEGMENT_SZ];

  while (remaining > 0 && !it.isNull()) {
    tmp[dstIdx] = *it.data;
    remaining--;
    dstIdx++;
    pattern.next(it);
    if (dstIdx == NDB_SECTION_SEGMENT_SZ || remaining == 0) {
      if (!appendToSection(ptrI, tmp, dstIdx)) {
        jam();
        return DbspjErr::OutOfSectionMemory;
      }
      dstIdx = 0;
    }
  }
  if (remaining > 0) {
    DEBUG_CRASH();
    return DbspjErr::InvalidPattern;
  } else {
    return 0;
  }
#endif
}

/**
 * This function takes a pattern and a row and expands it into a section
 */
Uint32 Dbspj::expand(Uint32 &_dst, Local_pattern_store &pattern,
                     const RowPtr &row, bool &hasNull) {
  Uint32 err;
  Uint32 dst = _dst;
  hasNull = false;
  Local_pattern_store::ConstDataBufferIterator it;
  pattern.first(it);
  while (!it.isNull()) {
    Uint32 info = *it.data;
    Uint32 type = QueryPattern::getType(info);
    Uint32 val = QueryPattern::getLength(info);
    pattern.next(it);
    switch (type) {
      case QueryPattern::P_COL:
        jam();
        err = appendColToSection(dst, row.m_row_data, val, hasNull);
        break;
      case QueryPattern::P_UNQ_PK:
        jam();
        err = appendPkColToSection(dst, row.m_row_data, val);
        break;
      case QueryPattern::P_ATTRINFO:
        jam();
        err = appendAttrinfoToSection(dst, row.m_row_data, val, hasNull);
        break;
      case QueryPattern::P_DATA:
        jam();
        err = appendDataToSection(dst, pattern, it, val, hasNull);
        break;
      case QueryPattern::P_PARENT:
        jam();
        // P_PARENT is a prefix to another pattern token
        // that permits code to access rows from earlier than immediate parent
        // val is no of levels to move up the tree
        err = appendFromParent(dst, pattern, it, val, row, hasNull);
        break;
        // PARAM's was converted to DATA by ::expand(pattern...)
      case QueryPattern::P_PARAM:
      case QueryPattern::P_PARAM_HEADER:
      default:
        jam();
        err = DbspjErr::InvalidPattern;
        DEBUG_CRASH();
    }
    if (unlikely(err != 0)) {
      jam();
      _dst = dst;
      return err;
    }
  }

  _dst = dst;
  return 0;
}

/* ::expand() used during initial 'build' phase on 'tree' + 'param' from API */
Uint32 Dbspj::expand(Uint32 &ptrI, DABuffer &pattern, Uint32 len,
                     DABuffer &param, Uint32 paramCnt, bool &hasNull) {
  jam();
  /**
   * TODO handle error
   */
  Uint32 err = 0;
  Uint32 tmp[1 + MAX_ATTRIBUTES_IN_TABLE];
  struct RowPtr::Row row;
  row.m_data = param.ptr;
  row.m_header = CAST_PTR(RowPtr::Header, &tmp[0]);
  buildRowHeader(CAST_PTR(RowPtr::Header, &tmp[0]), param.ptr, paramCnt);

  Uint32 dst = ptrI;
  const Uint32 *ptr = pattern.ptr;
  const Uint32 *end = ptr + len;
  hasNull = false;

  for (; ptr < end;) {
    Uint32 info = *ptr++;
    Uint32 type = QueryPattern::getType(info);
    Uint32 val = QueryPattern::getLength(info);
    switch (type) {
      case QueryPattern::P_PARAM:
        jam();
        ndbassert(val < paramCnt);
        err = appendColToSection(dst, row, val, hasNull);
        break;
      case QueryPattern::P_PARAM_HEADER:
        jam();
        ndbassert(val < paramCnt);
        err = appendAttrinfoToSection(dst, row, val, hasNull);
        break;
      case QueryPattern::P_DATA:
        if (unlikely(val == 0)) {
          jam();
          hasNull = true;
        } else if (likely(appendToSection(dst, ptr, val))) {
          jam();
          ptr += val;
        } else {
          jam();
          err = DbspjErr::OutOfSectionMemory;
        }
        break;
      case QueryPattern::P_COL:     // (linked) COL's not expected here
      case QueryPattern::P_PARENT:  // Prefix to P_COL
      case QueryPattern::P_ATTRINFO:
      case QueryPattern::P_UNQ_PK:
      default:
        jam();
        jamLine(type);
        err = DbspjErr::InvalidPattern;
    }
    if (unlikely(err != 0)) {
      jam();
      ptrI = dst;
      return err;
    }
  }

  /**
   * Iterate forward
   */
  pattern.ptr = end;
  ptrI = dst;
  return 0;
}

/* ::expand() used during initial 'build' phase on 'tree' + 'param' from API */
Uint32 Dbspj::expand(Local_pattern_store &dst, Ptr<TreeNode> treeNodePtr,
                     DABuffer &pattern, Uint32 len, DABuffer &param,
                     Uint32 paramCnt) {
  jam();
  /**
   * TODO handle error
   */
  Uint32 err;
  Uint32 tmp[1 + MAX_ATTRIBUTES_IN_TABLE];
  struct RowPtr::Row row;
  row.m_header = CAST_PTR(RowPtr::Header, &tmp[0]);
  row.m_data = param.ptr;
  buildRowHeader(CAST_PTR(RowPtr::Header, &tmp[0]), param.ptr, paramCnt);

  const Uint32 *end = pattern.ptr + len;
  for (; pattern.ptr < end;) {
    Uint32 info = *pattern.ptr;
    Uint32 type = QueryPattern::getType(info);
    Uint32 val = QueryPattern::getLength(info);
    switch (type) {
      case QueryPattern::P_COL:
      case QueryPattern::P_UNQ_PK:
      case QueryPattern::P_ATTRINFO:
        jam();
        err = appendToPattern(dst, pattern, 1);
        break;
      case QueryPattern::P_DATA:
        jam();
        err = appendToPattern(dst, pattern, val + 1);
        break;
      case QueryPattern::P_PARAM:
        jam();
        // NOTE: Converted to P_DATA by appendParamToPattern
        ndbassert(val < paramCnt);
        err = appendParamToPattern(dst, row, val);
        pattern.ptr++;
        break;
      case QueryPattern::P_PARAM_HEADER:
        jam();
        // NOTE: Converted to P_DATA by appendParamHeadToPattern
        ndbassert(val < paramCnt);
        err = appendParamHeadToPattern(dst, row, val);
        pattern.ptr++;
        break;
      case QueryPattern::P_PARENT:  // Prefix to P_COL
      {
        jam();
        err = appendToPattern(dst, pattern, 1);
        if (unlikely(err)) {
          jam();
          break;
        }
        // Locate requested grandparent and request it to
        // T_BUFFER_ROW its result rows
        Ptr<TreeNode> parentPtr;
        ndbrequire(
            m_treenode_pool.getPtr(parentPtr, treeNodePtr.p->m_parentPtrI));
        while (val--) {
          jam();
          ndbrequire(
              m_treenode_pool.getPtr(parentPtr, parentPtr.p->m_parentPtrI));
          parentPtr.p->m_bits |= TreeNode::T_BUFFER_ROW;
          parentPtr.p->m_bits |= TreeNode::T_BUFFER_MAP;
        }
        Ptr<Request> requestPtr;
        ndbrequire(
            m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI));
        break;
      }
      default:
        err = DbspjErr::InvalidPattern;
        jam();
    }

    if (unlikely(err != 0)) {
      jam();
      return err;
    }
  }
  return 0;
}

Uint32 Dbspj::parseDA(Build_context &ctx, Ptr<Request> requestPtr,
                      Ptr<TreeNode> treeNodePtr, DABuffer &tree,
                      const Uint32 treeBits, DABuffer &param,
                      const Uint32 paramBits) {
  Uint32 err;
  Uint32 attrInfoPtrI = RNIL;
  Uint32 attrParamPtrI = RNIL;

  do {
    /**
     * Test execution terminated due to 'OutOfSectionMemory' which
     * may happen multiple places (eg. appendtosection, expand) below:
     * - 17050: Fail on parseDA at first call
     * - 17051: Fail on parseDA if 'isLeaf'
     * - 17052: Fail on parseDA if treeNode not root
     * - 17053: Fail on parseDA at a random node of the query tree
     */
    if (ERROR_INSERTED(17050) ||
        (ERROR_INSERTED(17051) && (treeNodePtr.p->isLeaf())) ||
        (ERROR_INSERTED(17052) && (treeNodePtr.p->m_parentPtrI != RNIL)) ||
        (ERROR_INSERTED(17053) && (rand() % 7) == 0)) {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      g_eventLogger->info(
          "Injecting OutOfSectionMemory error at line %d file %s", __LINE__,
          __FILE__);
      err = DbspjErr::OutOfSectionMemory;
      break;
    }

    if (treeBits & DABits::NI_REPEAT_SCAN_RESULT) {
      jam();
      DEBUG("use REPEAT_SCAN_RESULT when returning results");
      requestPtr.p->m_bits |= Request::RT_REPEAT_SCAN_RESULT;
    }  // DABits::NI_REPEAT_SCAN_RESULT

    if (treeBits & DABits::NI_INNER_JOIN) {
      jam();
      DEBUG("INNER_JOIN optimization used");
      treeNodePtr.p->m_bits |= TreeNode::T_INNER_JOIN;
    }  // DABits::NI_INNER_JOIN

    if (treeBits & DABits::NI_FIRST_MATCH) {
      jam();
      DEBUG("FIRST_MATCH optimization used");
      treeNodePtr.p->m_bits |= TreeNode::T_FIRST_MATCH;
    }  // DABits::NI_FIRST_MATCH

    if (treeBits & DABits::NI_ANTI_JOIN) {
      jam();
      DEBUG("FIRST_MATCH optimization used for ANTI_JOIN");
      treeNodePtr.p->m_bits |= TreeNode::T_FIRST_MATCH;
    }  // DABits::NI_ANTI_JOIN

    if (treeBits & DABits::NI_HAS_PARENT) {
      jam();
      DEBUG("NI_HAS_PARENT");
      /**
       * OPTIONAL PART 1:
       *
       * Parent nodes are stored first in optional part
       *   this is a list of 16-bit numbers referring to
       *   *earlier* nodes in tree
       *   the list stores length of list as first 16-bit
       */
      err = DbspjErr::InvalidTreeNodeSpecification;
      Uint32 dst[63];
      Uint32 cnt = unpackList(NDB_ARRAY_SIZE(dst), dst, tree);
      if (unlikely(cnt > NDB_ARRAY_SIZE(dst))) {
        jam();
        break;
      }

      if (unlikely(cnt != 1)) {
        /**
         * Only a single parent supported for now, i.e only trees
         */
        jam();
        break;
      }

      err = 0;
      for (Uint32 i = 0; i < cnt; i++) {
        DEBUG("adding " << dst[i] << " as parent");
        Ptr<TreeNode> parentPtr = ctx.m_node_list[dst[i]];
        LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                                   m_dependency_map_pool);
        Local_dependency_map map(pool, parentPtr.p->m_child_nodes);
        if (unlikely(!map.append(&treeNodePtr.i, 1))) {
          err = DbspjErr::OutOfQueryMemory;
          jam();
          break;
        }
        treeNodePtr.p->m_parentPtrI = parentPtr.i;
      }

      if (unlikely(err != 0)) break;
    }  // DABits::NI_HAS_PARENT

    err = DbspjErr::InvalidTreeParametersSpecificationKeyParamBitsMissmatch;
    if (unlikely(((treeBits & DABits::NI_KEY_PARAMS) == 0) !=
                 ((paramBits & DABits::PI_KEY_PARAMS) == 0))) {
      jam();
      break;
    }

    if (treeBits & (DABits::NI_KEY_PARAMS | DABits::NI_KEY_LINKED |
                    DABits::NI_KEY_CONSTS)) {
      jam();
      DEBUG("NI_KEY_PARAMS | NI_KEY_LINKED | NI_KEY_CONSTS");

      /**
       * OPTIONAL PART 2:
       *
       * If keys are parametrized or linked
       *   DATA0[LO/HI] - Length of key pattern/#parameters to key
       */
      Uint32 len_cnt = *tree.ptr++;
      Uint32 len = len_cnt & 0xFFFF;  // length of pattern in words
      Uint32 cnt = len_cnt >> 16;     // no of parameters

      err = DbspjErr::InvalidTreeParametersSpecificationIncorrectKeyParamCount;
      if (unlikely(
              ((cnt == 0) != ((treeBits & DABits::NI_KEY_PARAMS) == 0)) ||
              ((cnt == 0) != ((paramBits & DABits::PI_KEY_PARAMS) == 0)))) {
        jam();
        break;
      }

      if (treeBits & DABits::NI_KEY_LINKED) {
        jam();
        LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                                   m_dependency_map_pool);
        Local_pattern_store pattern(pool, treeNodePtr.p->m_keyPattern);

        DEBUG("LINKED-KEY PATTERN w/ " << cnt << " PARAM values");
        /**
         * Expand pattern into a new pattern (with linked values)
         */
        err = expand(pattern, treeNodePtr, tree, len, param, cnt);
        if (unlikely(err != 0)) {
          jam();
          break;
        }
        /**
         * This node constructs a new key for each send
         */
        treeNodePtr.p->m_bits |= TreeNode::T_KEYINFO_CONSTRUCTED;
      } else {
        jam();
        DEBUG("FIXED-KEY w/ " << cnt << " PARAM values");
        /**
         * Expand pattern directly into keyinfo
         *   This means a "fixed" key from here on
         */
        bool hasNull;
        Uint32 keyInfoPtrI = RNIL;
        err = expand(keyInfoPtrI, tree, len, param, cnt, hasNull);
        if (unlikely(err != 0)) {
          jam();
          releaseSection(keyInfoPtrI);
          break;
        }
        if (unlikely(hasNull)) {
          /* API should have eliminated requests w/ const-NULL keys */
          jam();
          DEBUG("BEWARE: FIXED-key contain NULL values");
          releaseSection(keyInfoPtrI);
          //        treeNodePtr.p->m_bits |= TreeNode::T_NULL_PRUNE;
          //        break;
          ndbabort();
        }
        treeNodePtr.p->m_send.m_keyInfoPtrI = keyInfoPtrI;
      }
      ndbassert(err == 0);  // All errors should have been handled
    }                       // DABits::NI_KEY_...

    const Uint32 mask = DABits::NI_LINKED_ATTR | DABits::NI_ATTR_INTERPRET |
                        DABits::NI_ATTR_LINKED;

    if (((treeBits & mask) | (paramBits & DABits::PI_ATTR_LIST)) != 0) {
      jam();
      /**
       * OPTIONAL PART 3: attrinfo handling
       * - NI_LINKED_ATTR - these are attributes to be passed to children
       * - PI_ATTR_LIST   - this is "user-columns" (passed as parameters)

       * - NI_ATTR_INTERPRET - tree contains interpreted program
       * - NI_ATTR_LINKED - means that the attr-info contains linked-values
       *
       * IF NI_ATTR_INTERPRET
       *   DATA0[LO/HI] = Length of program / total #arguments to program
       *   DATA1..N     = Program
       *
       * IF PI_ATTR_INTERPRET
       *   DATA0[LO/HI] = Length of program / Length of subroutine-part
       *   DATA1..N     = Program (scan filter)
       *
       * IF NI_ATTR_LINKED
       *   DATA0[LO/HI] = Length / #
       */
      Uint32 *sectionptrs = nullptr;

      const bool interpreted =
          (treeBits & DABits::NI_ATTR_INTERPRET) ||
          (paramBits & DABits::PI_ATTR_INTERPRET) ||
          (treeNodePtr.p->m_bits & TreeNode::T_ATTR_INTERPRETED);

      if (interpreted) {
        static constexpr Uint32 sections[5] = {0, 0, 0, 0, 0};
        /**
         * Add section headers for interpreted execution
         *   and create pointer so that they can be updated later
         */
        jam();
        err = DbspjErr::OutOfSectionMemory;
        if (unlikely(!appendToSection(attrInfoPtrI, sections, 5))) {
          jam();
          break;
        }

        SegmentedSectionPtr ptr;
        getSection(ptr, attrInfoPtrI);
        sectionptrs = ptr.p->theData;

        /**
         * Note that there might be a NI_ATTR_LINKED without a
         * NI_ATTR_INTERPRET. INTERPRET code can then be specified with
         * PI_ATTR_INTERPRET. (or not)
         */
        if (treeBits & (DABits::NI_ATTR_INTERPRET | DABits::NI_ATTR_LINKED)) {
          jam();
          Uint32 len2 = *tree.ptr++;
          Uint32 len_prg = len2 & 0xFFFF;   // Length of interpret program
          Uint32 len_pattern = len2 >> 16;  // Length of attr param pattern

          // Note: NI_ATTR_INTERPRET seems to never have been used, nor tested
          if (treeBits & DABits::NI_ATTR_INTERPRET) {
            jam();
            /**
             * Having two interpreter programs is an error.
             */
            err = DbspjErr::BothTreeAndParametersContainInterpretedProgram;
            if (unlikely(paramBits & DABits::PI_ATTR_INTERPRET)) {
              jam();
              break;
            }

            treeNodePtr.p->m_bits |= TreeNode::T_ATTR_INTERPRETED;
            err = DbspjErr::OutOfSectionMemory;
            if (unlikely(!appendToSection(attrInfoPtrI, tree.ptr, len_prg))) {
              jam();
              break;
            }
            tree.ptr += len_prg;
            sectionptrs[1] = len_prg;  // size of interpret program
          }                            // NI_ATTR_INTERPRET

          /**
           * We do not support (or need) API supplied parameters to
           * be expand'ed into the interpreter parameter section.
           * Such parameters has always been included directly into the
           * generated interpreter code. Thus the no_param being set up
           * as expand() arguemt here.
           */
          DABuffer no_param;
          no_param.ptr = nullptr;

          if (treeBits & DABits::NI_ATTR_LINKED) {
            jam();
            DEBUG("NI_ATTR_LINKED"
                  << ", len_pattern:" << len_pattern);
            /**
             * Expand pattern into a new pattern (with linked values)
             * Real attrInfo will be constructed with another expand
             * when parent row values arrives.
             */
            LocalArenaPool<DataBufferSegment<14>> pool(requestPtr.p->m_arena,
                                                       m_dependency_map_pool);
            Local_pattern_store pattern(pool,
                                        treeNodePtr.p->m_attrParamPattern);
            err = expand(pattern, treeNodePtr, tree, len_pattern, no_param, 0);
            if (unlikely(err)) {
              jam();
              break;
            }
            /**
             * This node constructs a new attr-info for each send
             */
            treeNodePtr.p->m_bits |= TreeNode::T_ATTRINFO_CONSTRUCTED;
          } else if (len_pattern > 0) {
            jam();
            // This code branch has never been tested, unused as well.
            ndbassert(false);  // Need validation before being used.

            /**
             * Expand pattern directly into attr-info param
             *   This means a "fixed" attr-info param from here on
             */
            bool hasNull;
            err =
                expand(attrParamPtrI, tree, len_pattern, no_param, 0, hasNull);
            if (unlikely(err)) {
              jam();
              break;
            }
          }  // NI_ATTR_LINKED
        }    // NI_ATTR_INTERPRET | NI_ATTR_LINKED

        /**
         * Interpreter code may also be (usually is) specified in the
         * param-section. That might be combined with a NI_ATTR_LINKED
         * containing the constructor receipe for a parameter.
         */
        if (paramBits & DABits::PI_ATTR_INTERPRET) {
          jam();
          DEBUG("PI_ATTR_INTERPRET");

          /**
           * Add the interpreted code that represents the scan filter.
           */
          const Uint32 len2 = *param.ptr++;
          const Uint32 program_len = len2 & 0xFFFF;
          const Uint32 subroutine_len = len2 >> 16;
          err = DbspjErr::OutOfSectionMemory;
          if (unlikely(
                  !appendToSection(attrInfoPtrI, param.ptr, program_len))) {
            jam();
            break;
          }
          /**
           * The interpreted code is added is in the "Interpreted execute
           * region" of the attrinfo (see Dbtup::interpreterStartLab() for
           * details). It will thus execute before reading the attributes that
           * constitutes the projections.
           */
          sectionptrs[1] = program_len;
          param.ptr += program_len;

          if (subroutine_len > 0) {
            jam();
            // This code branch has never been tested, unused as well.
            ndbassert(false);  // Need validation before being used.

            err = DbspjErr::OutOfSectionMemory;
            if (unlikely(!appendToSection(attrParamPtrI, param.ptr,
                                          subroutine_len))) {
              jam();
              break;
            }
            sectionptrs[4] = subroutine_len;
            param.ptr += subroutine_len;
          }
          treeNodePtr.p->m_bits |= TreeNode::T_ATTR_INTERPRETED;
        } else  // not PI_ATTR_INTERPRET
        {
          jam();
          treeNodePtr.p->m_bits |= TreeNode::T_ATTR_INTERPRETED;

          if (!(treeBits & DABits::NI_ATTR_INTERPRET)) {
            jam();

            /**
             * Tree node has interpreted execution,
             *   but no interpreted program specified
             *   auto-add Exit_ok (i.e return each row)
             */
            Uint32 tmp = Interpreter::ExitOK();
            err = DbspjErr::OutOfSectionMemory;
            if (unlikely(!appendToSection(attrInfoPtrI, &tmp, 1))) {
              jam();
              break;
            }
            sectionptrs[1] = 1;
          }
        }  // PI_ATTR_INTERPRET)
      }    // if (interpreted)

      Uint32 sum_read = 0;
      Uint32 dst[MAX_ATTRIBUTES_IN_TABLE + 2];

      if (paramBits & DABits::PI_ATTR_LIST) {
        jam();
        Uint32 len = *param.ptr++;
        DEBUG("PI_ATTR_LIST");

        treeNodePtr.p->m_bits |= TreeNode::T_USER_PROJECTION;
        err = DbspjErr::OutOfSectionMemory;
        if (!appendToSection(attrInfoPtrI, param.ptr, len)) {
          jam();
          break;
        }

        param.ptr += len;
        sum_read += len;

        const NodeId API_node = refToNode(ctx.m_resultRef);
        const Uint32 API_version = getNodeInfo(API_node).m_version;

        /**
         * We have just added a 'USER_PROJECTION' which is the
         * result row to the SPJ-API. If we will also add a
         * projection of SPJ keys (NI_LINKED_ATTR), we need to
         * insert a FLUSH of the client results now, else the
         * FLUSH is skipped as we produced a single result
         * projection only. (to API client)
         *
         * However, for scan requests we will always need to FLUSH:
         * LqhKeyReq::tcBlockref need to refer this SPJ block as
         * it is used to send the required REF/CONF to SPJ. However,
         * tcBlockref is also used as the 'route' dest for TRANSID_AI_R,
         * which should be routed to the requesting TC block. Thus
         * we need the FLUSH which specifies its own RouteRef.
         *
         * Also need to have this under API-version control, as
         * older API versions assumed that all SPJ results were
         * returned as 'long' signals.
         */
        if (treeBits & DABits::NI_LINKED_ATTR || requestPtr.p->isScan() ||
            !ndbd_spj_api_support_short_TRANSID_AI(API_version)) {
          /**
           * Insert a FLUSH_AI of 'USER_PROJECTION' result (to client)
           * before 'LINKED_ATTR' results to SPJ is produced.
           */
          jam();
          Uint32 flush[4];
          flush[0] = AttributeHeader::FLUSH_AI << 16;
          flush[1] = ctx.m_resultRef;
          flush[2] = ctx.m_resultData;
          flush[3] = ctx.m_senderRef;  // RouteRef
          if (!appendToSection(attrInfoPtrI, flush, 4)) {
            jam();
            break;
          }
          sum_read += 4;
        }
      }

      if (treeBits & DABits::NI_LINKED_ATTR) {
        jam();
        DEBUG("NI_LINKED_ATTR");
        err = DbspjErr::InvalidTreeNodeSpecification;
        Uint32 cnt = unpackList(MAX_ATTRIBUTES_IN_TABLE, dst, tree);
        if (unlikely(cnt > MAX_ATTRIBUTES_IN_TABLE)) {
          jam();
          break;
        }

        /**
         * AttributeHeader contains attrId in 16-higher bits
         */
        for (Uint32 i = 0; i < cnt; i++) dst[i] <<= 16;

        /**
         * Read correlation factor
         */
        dst[cnt++] = AttributeHeader::CORR_FACTOR32 << 16;

        err = DbspjErr::OutOfSectionMemory;
        if (!appendToSection(attrInfoPtrI, dst, cnt)) {
          jam();
          break;
        }
        sum_read += cnt;
        treeNodePtr.p->m_bits |= TreeNode::T_EXPECT_TRANSID_AI;

        // Having a key projection for LINKED child, implies not-LEAF
        treeNodePtr.p->m_bits &= ~(Uint32)TreeNode::T_LEAF;
      }
      /**
       * If no LINKED_ATTR's including the CORR_FACTOR was requested by
       * the API, the SPJ-block does its own request of a CORR_FACTOR.
       * Will be used to keep track of whether a 'match' was found
       * for the requested parent row.
       */
      else if (requestPtr.p->isScan() &&
               (treeNodePtr.p->m_bits &
                (TreeNode::T_INNER_JOIN | TreeNode::T_FIRST_MATCH))) {
        jam();
        Uint32 cnt = 0;
        /**
         * Only read correlation factor
         */
        dst[cnt++] = AttributeHeader::CORR_FACTOR32 << 16;

        err = DbspjErr::OutOfSectionMemory;
        if (!appendToSection(attrInfoPtrI, dst, cnt)) {
          jam();
          break;
        }
        sum_read += cnt;
        treeNodePtr.p->m_bits |= TreeNode::T_EXPECT_TRANSID_AI;
      }

      if (interpreted) {
        jam();
        /**
         * Let reads be performed *after* interpreted program
         *   i.e in "final read"-section
         */
        sectionptrs[3] = sum_read;

        if (attrParamPtrI != RNIL) {
          jam();
          ndbrequire(
              !(treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED));

          SegmentedSectionPtr ptr;
          getSection(ptr, attrParamPtrI);
          {
            SectionReader r0(ptr, getSectionSegmentPool());
            err = appendReaderToSection(attrInfoPtrI, r0, ptr.sz);
            if (unlikely(err != 0)) {
              jam();
              break;
            }
            sectionptrs[4] = ptr.sz;
          }
          releaseSection(attrParamPtrI);
          attrParamPtrI = RNIL;
        }
      }

      treeNodePtr.p->m_send.m_attrInfoPtrI = attrInfoPtrI;
      attrInfoPtrI = RNIL;
    }  // if (((treeBits & mask) | (paramBits & DABits::PI_ATTR_LIST)) != 0)

    // Empty attrinfo would cause node crash.
    if (treeNodePtr.p->m_send.m_attrInfoPtrI == RNIL) {
      jam();

      // Add dummy interpreted program.
      Uint32 tmp = Interpreter::ExitOK();
      err = DbspjErr::OutOfSectionMemory;
      if (unlikely(!appendToSection(treeNodePtr.p->m_send.m_attrInfoPtrI, &tmp,
                                    1))) {
        jam();
        break;
      }
    }

    return 0;
  } while (0);

  if (attrInfoPtrI != RNIL) {
    jam();
    releaseSection(attrInfoPtrI);
  }

  if (attrParamPtrI != RNIL) {
    jam();
    releaseSection(attrParamPtrI);
  }

  return err;
}

/**
 * END - MODULE COMMON PARSE/UNPACK
 */

/**
 * Process a scan request for an ndb$info table. (These are used for monitoring
 * purposes and do not contain application data.)
 */
void Dbspj::execDBINFO_SCANREQ(Signal *signal) {
  DbinfoScanReq req = *CAST_PTR(DbinfoScanReq, &signal->theData[0]);
  const Ndbinfo::ScanCursor *cursor =
      CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch (req.tableId) {
      // The SPJ block only implements the ndbinfo.counters table.
    case Ndbinfo::COUNTERS_TABLEID: {
      Ndbinfo::counter_entry counters[] = {
          {Ndbinfo::SPJ_READS_RECEIVED_COUNTER,
           c_Counters.get_counter(CI_READS_RECEIVED)},
          {Ndbinfo::SPJ_LOCAL_READS_SENT_COUNTER,
           c_Counters.get_counter(CI_LOCAL_READS_SENT)},
          {Ndbinfo::SPJ_REMOTE_READS_SENT_COUNTER,
           c_Counters.get_counter(CI_REMOTE_READS_SENT)},
          {Ndbinfo::SPJ_READS_NOT_FOUND_COUNTER,
           c_Counters.get_counter(CI_READS_NOT_FOUND)},
          {Ndbinfo::SPJ_TABLE_SCANS_RECEIVED_COUNTER,
           c_Counters.get_counter(CI_TABLE_SCANS_RECEIVED)},
          {Ndbinfo::SPJ_LOCAL_TABLE_SCANS_SENT_COUNTER,
           c_Counters.get_counter(CI_LOCAL_TABLE_SCANS_SENT)},
          {Ndbinfo::SPJ_RANGE_SCANS_RECEIVED_COUNTER,
           c_Counters.get_counter(CI_RANGE_SCANS_RECEIVED)},
          {Ndbinfo::SPJ_LOCAL_RANGE_SCANS_SENT_COUNTER,
           c_Counters.get_counter(CI_LOCAL_RANGE_SCANS_SENT)},
          {Ndbinfo::SPJ_REMOTE_RANGE_SCANS_SENT_COUNTER,
           c_Counters.get_counter(CI_REMOTE_RANGE_SCANS_SENT)},
          {Ndbinfo::SPJ_SCAN_BATCHES_RETURNED_COUNTER,
           c_Counters.get_counter(CI_SCAN_BATCHES_RETURNED)},
          {Ndbinfo::SPJ_SCAN_ROWS_RETURNED_COUNTER,
           c_Counters.get_counter(CI_SCAN_ROWS_RETURNED)},
          {Ndbinfo::SPJ_PRUNED_RANGE_SCANS_RECEIVED_COUNTER,
           c_Counters.get_counter(CI_PRUNED_RANGE_SCANS_RECEIVED)},
          {Ndbinfo::SPJ_CONST_PRUNED_RANGE_SCANS_RECEIVED_COUNTER,
           c_Counters.get_counter(CI_CONST_PRUNED_RANGE_SCANS_RECEIVED)}};
      const size_t num_counters = sizeof(counters) / sizeof(counters[0]);

      Uint32 i = cursor->data[0];
      const BlockNumber bn = blockToMain(number());
      while (i < num_counters) {
        jam();
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(bn);          // block number
        row.write_uint32(instance());  // block instance
        row.write_uint32(counters[i].id);

        row.write_uint64(counters[i].val);
        ndbinfo_send_row(signal, req, row, rl);
        i++;
        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, i);
          return;
        }
      }
      break;
    }

    default:
      break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}  // Dbspj::execDBINFO_SCANREQ(Signal *signal)
