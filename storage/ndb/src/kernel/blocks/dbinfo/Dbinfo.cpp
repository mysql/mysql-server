/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "Dbinfo.hpp"

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/TransIdAI.hpp>

//#define DBINFO_SCAN_TRACE
#ifdef DBINFO_SCAN_TRACE
#include <debugger/DebuggerNames.hpp>
#endif

#define JAM_FILE_ID 455

Uint32 dbinfo_blocks[] = {DBACC,  DBTUP,  BACKUP, DBTC,  SUMA,  DBUTIL, TRIX,
                          DBTUX,  DBDICT, CMVMI,  DBLQH, LGMAN, PGMAN,  DBSPJ,
                          THRMAN, TRPMAN, QMGR,   DBDIH, 0};

Dbinfo::Dbinfo(Block_context &ctx) : SimulatedBlock(DBINFO, ctx) {
  BLOCK_CONSTRUCTOR(Dbinfo);

  static_assert(sizeof(DbinfoScanCursor) == sizeof(Ndbinfo::ScanCursor));

  /* Add Received Signals */
  addRecSignal(GSN_STTOR, &Dbinfo::execSTTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &Dbinfo::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbinfo::execREAD_CONFIG_REQ, true);

  addRecSignal(GSN_DBINFO_SCANREQ, &Dbinfo::execDBINFO_SCANREQ);
  addRecSignal(GSN_DBINFO_SCANCONF, &Dbinfo::execDBINFO_SCANCONF);

  addRecSignal(GSN_NODE_FAILREP, &Dbinfo::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ, &Dbinfo::execINCL_NODEREQ);
}

Dbinfo::~Dbinfo() {}

BLOCK_FUNCTIONS(Dbinfo)

void Dbinfo::execSTTOR(Signal *signal) {
  jamEntry();
  sendSTTORRY(signal);
  return;
}

void Dbinfo::execREAD_CONFIG_REQ(Signal *signal) {
  jamEntry();
  const ReadConfigReq *req = (ReadConfigReq *)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  Uint32 ntable;

  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_TABLES, &ntable));

  // Estimated number of tables. Without actually counting the tables in dict,
  // this estimate could be far off. Take the configured maximum and divide
  // by 3; then any actual value in the range from 11% to 100% will be off by
  // a factor of 3 at most.
  counts.est_tables = ntable / 3;

  // Count nodes
  for (int i = 1; i < MAX_NODES; i++) {
    NodeInfo::NodeType type = getNodeInfo(i).getType();
    if (type == NodeInfo::NodeType::DB) counts.data_nodes++;
    if (type == NodeInfo::NodeType::DB || type == NodeInfo::NodeType::API ||
        type == NodeInfo::NodeType::MGM)
      counts.all_nodes++;
  }

  // Count threads
  const THRConfigApplier &thr_cf =
      globalEmulatorData.theConfiguration->m_thr_config;
  counts.threads.send = globalData.ndbMtSendThreads;
  counts.threads.db = thr_cf.getThreadCount();
  counts.threads.ldm = thr_cf.getThreadCount(THRConfig::T_LDM);
  counts.cpus = Ndb_GetHWInfo(false)->cpu_cnt;

  // Count block instances
  counts.log_parts = globalData.ndbLogParts;
  counts.instances.tc = globalData.ndbMtTcWorkers;
  counts.instances.lqh = globalData.ndbMtLqhWorkers;
  counts.instances.pgman = counts.instances.lqh + 1;

  // Send conf
  ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, ReadConfigConf::SignalLength,
             JBB);
}

void Dbinfo::sendSTTORRY(Signal *signal) {
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 255;  // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
}

void Dbinfo::execDUMP_STATE_ORD(Signal *signal) {
  jamEntry();

  switch (signal->theData[0]) {
    case DumpStateOrd::DbinfoListTables:
      jam();
      g_eventLogger->info("--- BEGIN NDB$INFO.TABLES ---");
      for (int i = 0; i < Ndbinfo::getNumTableEntries(); i++) {
        const Ndbinfo::Table *tab = Ndbinfo::getTable(i);
        if (tab == nullptr) continue;
        g_eventLogger->info("%d,%s", i, tab->m.name);
      }
      g_eventLogger->info("--- END NDB$INFO.TABLES ---");
      break;

    case DumpStateOrd::DbinfoListColumns:
      jam();
      g_eventLogger->info("--- BEGIN NDB$INFO.COLUMNS ---");
      for (int i = 0; i < Ndbinfo::getNumTableEntries(); i++) {
        const Ndbinfo::Table *tab = Ndbinfo::getTable(i);
        if (tab == nullptr) continue;

        for (int j = 0; j < tab->m.ncols; j++)
          g_eventLogger->info("%d,%d,%s,%d", i, j, tab->col[j].name,
                              tab->col[j].coltype);
      }
      g_eventLogger->info("--- END NDB$INFO.COLUMNS ---");
      break;
  };
}

Uint32 Dbinfo::find_next_block(Uint32 block) const {
  int i = 0;
  // Find current blocks position
  while (dbinfo_blocks[i] != block && dbinfo_blocks[i] != 0) i++;

  // Make sure current block was found
  ndbrequire(dbinfo_blocks[i]);

  // Return the next block(which might be 0)
  return dbinfo_blocks[++i];
}

static Uint32 switchRef(Uint32 block, Uint32 node) {
  const Uint32 ref = numberToRef(block, node);
#ifdef DBINFO_SCAN_TRACE
  g_eventLogger->info("Dbinfo: switching to %s in node %d, ref: 0x%.8x",
                      getBlockName(block, "<unknown>"), node, ref);
#endif
  return ref;
}

bool Dbinfo::find_next(Ndbinfo::ScanCursor *cursor) const {
  Uint32 node = refToNode(cursor->currRef);
  Uint32 block = refToBlock(cursor->currRef);
  const Uint32 instance = refToInstance(cursor->currRef);
  ndbrequire(instance == 0);

  if (node == 0) {
    jam();
    // First 'find_next'
    ndbrequire(block == 0);
    cursor->currRef = switchRef(dbinfo_blocks[0], getOwnNodeId());
    return true;
  }

  if (block) {
    jam();
    // Find next block
    ndbrequire(node == getOwnNodeId());
    block = find_next_block(block);
    if (block) {
      jam();
      cursor->currRef = switchRef(block, node);
      return true;
    }
  }

  // Nothing more to scan
  cursor->currRef = 0;
  return false;
}

void Dbinfo::execDBINFO_SCANREQ(Signal *signal) {
  jamEntry();
  DbinfoScanReq *req_ptr = (DbinfoScanReq *)signal->getDataPtrSend();
  const Uint32 senderRef = signal->header.theSendersBlockRef;

  // Copy signal on stack
  DbinfoScanReq req = *req_ptr;

  const Uint32 resultData = req.resultData;
  const Uint32 transId0 = req.transId[0];
  const Uint32 transId1 = req.transId[1];
  const Uint32 resultRef = req.resultRef;

  // Validate tableId
  const Uint32 tableId = req.tableId;
  if (tableId >= (Uint32)Ndbinfo::getNumTableEntries()) {
    jam();
    DbinfoScanRef *ref = (DbinfoScanRef *)signal->getDataPtrSend();
    ref->resultData = resultData;
    ref->transId[0] = transId0;
    ref->transId[1] = transId1;
    ref->resultRef = resultRef;
    ref->errorCode = DbinfoScanRef::NoTable;
    sendSignal(senderRef, GSN_DBINFO_SCANREF, signal,
               DbinfoScanRef::SignalLength, JBB);
    return;
  }

  // TODO Check all scan parameters
  Ndbinfo::ScanCursor *cursor =
      CAST_PTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtrSend(&req));

  Uint32 signal_length = signal->getLength();
  if (signal_length == DbinfoScanReq::SignalLength) {
    // Initialize cursor
    jam();
    cursor->senderRef = senderRef;
    cursor->saveSenderRef = 0;
    cursor->currRef = 0;
    cursor->saveCurrRef = 0;
    // Reset all data holders
    memset(cursor->data, 0, sizeof(cursor->data));
    cursor->flags = 0;
    cursor->totalRows = 0;
    cursor->totalBytes = 0;
    req.cursor_sz = Ndbinfo::ScanCursor::Length;
    signal_length += req.cursor_sz;
  }
  ndbrequire(signal_length ==
             DbinfoScanReq::SignalLength + Ndbinfo::ScanCursor::Length);
  ndbrequire(req.cursor_sz == Ndbinfo::ScanCursor::Length);

  switch (tableId) {
    case Ndbinfo::TABLES_TABLEID: {
      jam();

      Ndbinfo::Ratelimit rl;
      Uint32 tableId = cursor->data[0];

      while (tableId < (Uint32)Ndbinfo::getNumTableEntries()) {
        jam();
        const Ndbinfo::Table *tab = Ndbinfo::getTable(tableId);
        if (tab == nullptr) {
          tableId++;
          continue;
        }
        Ndbinfo::Row row(signal, req);
        row.write_uint32(tableId);
        row.write_string(tab->m.name);
        row.write_string(tab->m.comment);
        row.write_uint32(tab->m.estimate_rows(counts));
        ndbinfo_send_row(signal, req, row, rl);

        tableId++;

        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, tableId);
          return;
        }
      }

      // All tables sent
      req.cursor_sz = 0;  // Close cursor
      ndbinfo_send_scan_conf(signal, req, rl);
      return;

      break;
    }

    case Ndbinfo::COLUMNS_TABLEID: {
      jam();

      Ndbinfo::Ratelimit rl;
      Uint32 tableId = cursor->data[0];
      Uint32 columnId = cursor->data[1];

      while (tableId < (Uint32)Ndbinfo::getNumTableEntries()) {
        jam();
        const Ndbinfo::Table *tab = Ndbinfo::getTable(tableId);
        if (tab == nullptr) {
          columnId = 0;
          tableId++;
          continue;
        }
        while (columnId < (Uint32)tab->m.ncols) {
          jam();
          Ndbinfo::Row row(signal, req);
          row.write_uint32(tableId);
          row.write_uint32(columnId);
          row.write_string(tab->col[columnId].name);
          row.write_uint32(tab->col[columnId].coltype);
          row.write_string(tab->col[columnId].comment);
          ndbinfo_send_row(signal, req, row, rl);

          assert(columnId < 256);
          columnId++;

          if (rl.need_break(req)) {
            jam();
            ndbinfo_send_scan_break(signal, req, rl, tableId, columnId);
            return;
          }
        }
        columnId = 0;
        tableId++;
      }

      // All tables and columns sent
      req.cursor_sz = 0;  // Close cursor
      ndbinfo_send_scan_conf(signal, req, rl);

      break;
    }

    default: {
      jam();

      ndbassert(tableId > 1);

      // printSignalHeader(stdout, signal->header, 99, 98, true);
      // printDBINFO_SCAN(stdout, signal->theData, signal->getLength(), 0);

      if (Ndbinfo::ScanCursor::getHasMoreData(cursor->flags) ||
          find_next(cursor)) {
        jam();
        ndbrequire(cursor->currRef);

        // CONF or REF should be sent back here
        cursor->senderRef = reference();

        // Send SCANREQ
        MEMCOPY_NO_WORDS(req_ptr, &req, signal_length);
        sendSignal(cursor->currRef, GSN_DBINFO_SCANREQ, signal, signal_length,
                   JBB);
      } else {
        // Scan is done, send SCANCONF back to caller
        jam();
        DbinfoScanConf *apiconf = (DbinfoScanConf *)signal->getDataPtrSend();
        MEMCOPY_NO_WORDS(apiconf, &req, DbinfoScanConf::SignalLength);
        // Set cursor_sz back to 0 to indicate end of scan
        apiconf->cursor_sz = 0;
        sendSignal(resultRef, GSN_DBINFO_SCANCONF, signal,
                   DbinfoScanConf::SignalLength, JBB);
      }
      break;
    }
  }
}

void Dbinfo::execDBINFO_SCANCONF(Signal *signal) {
  const DbinfoScanConf *conf_ptr = (const DbinfoScanConf *)signal->getDataPtr();
  // Copy signal on stack
  DbinfoScanConf conf = *conf_ptr;

  jamEntry();

  // printDBINFO_SCAN(stdout, signal->theData, signal->getLength(), 0);

  Uint32 signal_length = signal->getLength();
  ndbrequire(signal_length ==
             DbinfoScanReq::SignalLength + Ndbinfo::ScanCursor::Length);
  ndbrequire(conf.cursor_sz == Ndbinfo::ScanCursor::Length);

  // Validate tableId
  ndbassert(conf.tableId < (Uint32)Ndbinfo::getNumTableEntries());

  const Uint32 resultRef = conf.resultRef;

  // Copy cursor on stack
  ndbrequire(conf.cursor_sz);
  Ndbinfo::ScanCursor *cursor =
      CAST_PTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtrSend(&conf));

  if (Ndbinfo::ScanCursor::getHasMoreData(cursor->flags) || conf.returnedRows) {
    // Rate limit break, pass through to API
    jam();
    ndbrequire(cursor->currRef);
    DbinfoScanConf *apiconf = (DbinfoScanConf *)signal->getDataPtrSend();
    MEMCOPY_NO_WORDS(apiconf, &conf, signal_length);
    sendSignal(resultRef, GSN_DBINFO_SCANCONF, signal, signal_length, JBB);
    return;
  }

  if (find_next(cursor)) {
    jam();
    ndbrequire(cursor->currRef);

    // CONF or REF should be sent back here
    cursor->senderRef = reference();

    // Send SCANREQ
    MEMCOPY_NO_WORDS(signal->getDataPtrSend(), &conf, signal_length);
    sendSignal(cursor->currRef, GSN_DBINFO_SCANREQ, signal, signal_length, JBB);
    return;
  }

  // Scan is done, send SCANCONF back to caller
  jam();
  DbinfoScanConf *apiconf = (DbinfoScanConf *)signal->getDataPtrSend();
  MEMCOPY_NO_WORDS(apiconf, &conf, DbinfoScanConf::SignalLength);

  // Set cursor_sz back to 0 to indicate end of scan
  apiconf->cursor_sz = 0;
  sendSignal(resultRef, GSN_DBINFO_SCANCONF, signal,
             DbinfoScanConf::SignalLength, JBB);
  return;
}

void Dbinfo::execINCL_NODEREQ(Signal *signal) {
  jamEntry();

  const Uint32 senderRef = signal->theData[0];
  const Uint32 inclNode = signal->theData[1];

  signal->theData[0] = inclNode;
  signal->theData[1] = reference();
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 2, JBB);
}

void Dbinfo::execNODE_FAILREP(Signal *signal) {
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
  Uint32 theFailedNodes[NdbNodeBitmask::Size];
  for (Uint32 i = 0; i < NdbNodeBitmask::Size; i++)
    theFailedNodes[i] = rep->theNodes[i];

  for (Uint32 i = 0; i < MAX_NDB_NODES; i++) {
    if (NdbNodeBitmask::get(theFailedNodes, i)) {
      Uint32 elementsCleaned = simBlockNodeFailure(signal, i);  // No callback
      ndbassert(elementsCleaned ==
                0);           // DbInfo should have no distributed frag signals
      (void)elementsCleaned;  // Remove compiler warning
    }
  }
}
