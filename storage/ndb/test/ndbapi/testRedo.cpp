/*
   Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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

#include <time.h>
#include <cstdint>
#include "util/require.h"

#include <NdbSleep.h>
#include <NdbTick.h>
#include <mgmapi.h>
#include <ndb_logevent.h>
#include <random.h>
#include <HugoOperations.hpp>
#include <NDBT.hpp>
#include <NDBT_Stats.hpp>
#include <NDBT_Test.hpp>
#include <NdbMgmd.hpp>
#include <NdbRestarter.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include "../../src/ndbapi/NdbInfo.hpp"

static NdbMutex *g_msgmutex = 0;

#undef require
#define require(b)                                                      \
  if (!(b)) {                                                           \
    NdbMutex_Lock(g_msgmutex);                                          \
    g_err << "ABORT: " << #b << " failed at line " << __LINE__ << endl; \
    NdbMutex_Unlock(g_msgmutex);                                        \
    abort();                                                            \
  }

#define CHK1(b)                                                         \
  if (!(b)) {                                                           \
    NdbMutex_Lock(g_msgmutex);                                          \
    g_err << "ERROR: " << #b << " failed at line " << __LINE__ << endl; \
    NdbMutex_Unlock(g_msgmutex);                                        \
    result = NDBT_FAILED;                                               \
    break;                                                              \
  }

#define CHK2(b, e)                                                          \
  if (!(b)) {                                                               \
    NdbMutex_Lock(g_msgmutex);                                              \
    g_err << "ERROR: " << #b << " failed at line " << __LINE__ << ": " << e \
          << endl;                                                          \
    NdbMutex_Unlock(g_msgmutex);                                            \
    result = NDBT_FAILED;                                                   \
    break;                                                                  \
  }

#define CHK3(b, e)                                                          \
  if (!(b)) {                                                               \
    NdbMutex_Lock(g_msgmutex);                                              \
    g_err << "ERROR: " << #b << " failed at line " << __LINE__ << ": " << e \
          << endl;                                                          \
    NdbMutex_Unlock(g_msgmutex);                                            \
    return NDBT_FAILED;                                                     \
  }

#define info(x)                  \
  do {                           \
    NdbMutex_Lock(g_msgmutex);   \
    g_info << x << endl;         \
    NdbMutex_Unlock(g_msgmutex); \
  } while (0)

static const int g_tabmax = 3;
static const char *g_tabname[g_tabmax] = {"T1", "T2", "T4"};
static const NdbDictionary::Table *g_tabptr[g_tabmax] = {0, 0, 0};

static int runCreate(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  pNdb->waitUntilReady();
  NdbDictionary::Dictionary *pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  int tabmask = ctx->getProperty("TABMASK", (Uint32)0);

  for (int i = 0; i < g_tabmax; i++) {
    if (!(tabmask & (1 << i))) continue;
    const char *tabname = g_tabname[i];
    (void)pDic->dropTable(tabname);

    const NdbDictionary::Table *pTab = NDBT_Tables::getTable(tabname);
    require(pTab != 0);
    NdbDictionary::Table tab2(*pTab);
    // make sure to hit all log parts
    tab2.setFragmentType(NdbDictionary::Object::FragAllLarge);
    // tab2.setMaxRows(100000000);
    CHK2(pDic->createTable(tab2) == 0, pDic->getNdbError());

    g_tabptr[i] = pDic->getTable(tabname);
    require(g_tabptr[i] != 0);
    info("created " << tabname);
  }

  return result;
}

static int runDrop(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  int tabmask = ctx->getProperty("TABMASK", (Uint32)0);

  for (int i = 0; i < g_tabmax; i++) {
    if (!(tabmask & (1 << i))) continue;
    const char *tabname = g_tabname[i];
    if (g_tabptr[i] != 0) {
      CHK2(pDic->dropTable(tabname) == 0, pDic->getNdbError());
      g_tabptr[i] = 0;
      info("dropped " << tabname);
    }
  }

  return result;
}

// 0-writer has not seen 410 error
// 1-writer sees 410 error
// 2-longtrans has rolled back

static int get_err410(NDBT_Context *ctx) {
  int v = (int)ctx->getProperty("ERR410", (Uint32)0);
  require(v == 0 || v == 1 || v == 2);
  return v;
}

static void set_err410(NDBT_Context *ctx, int v) {
  require(v == 0 || v == 1 || v == 2);
  require(get_err410(ctx) != v);
  ctx->setProperty("ERR410", (Uint32)v);
}

static int runLongtrans(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  int result = NDBT_OK;
  const int sleep410 = ctx->getProperty("SLEEP410", (Uint32)0);

  const NdbDictionary::Table *pTab = g_tabptr[0];
  require(pTab != 0);

  info("longtrans: start");
  int loop = 0;
  while (!ctx->isTestStopped()) {
    info("longtrans: loop " << loop);
    HugoOperations ops(*pTab);
    ops.setQuiet();
    CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());
    CHK2(ops.pkInsertRecord(pNdb, 0, 1, 0) == 0, ops.getNdbError());

    while (!ctx->isTestStopped()) {
      int v = get_err410(ctx);
      require(v == 0 || v == 1);
      if (v != 0) {
        info("longtrans: 410 seen");
        if (sleep410 > 0) {
          info("longtrans: sleep " << sleep410);
          NdbSleep_SecSleep(sleep410);
        }

        CHK2(ops.execute_Rollback(pNdb) == 0, ops.getNdbError());
        ops.closeTransaction(pNdb);
        info("longtrans: rollback done");
        set_err410(ctx, 2);

        while (!ctx->isTestStopped()) {
          int v = get_err410(ctx);
          if (v != 0) {
          } else {
            info("longtrans: 410 cleared");
            break;
          }
          NdbSleep_SecSleep(1);
        }
        break;
      }
      NdbSleep_SecSleep(1);
    }
    CHK1(result == NDBT_OK);

    if (ops.getTransaction() != NULL) {
      info("longtrans: close leftover transaction");
      ops.closeTransaction(pNdb);
    }

    loop++;
  }

  info("longtrans: stop");
  return result;
}

static int run_write_ops(NDBT_Context *ctx, NDBT_Step *step, int upval,
                         NdbError &err, bool abort_on_error = false) {
  Ndb *pNdb = GETNDB(step);
  const int records = ctx->getNumRecords();
  int result = NDBT_OK;

  const NdbDictionary::Table *pTab = g_tabptr[1];
  require(pTab != 0);
  int startRecord = 0;
  int stopRecord = records;
  if (ctx->getProperty("RANGE_PER_STEP", (Uint32)0) != 0) {
    NDBT_Context::getRecordSubRange(records, step->getStepTypeCount(),
                                    step->getStepTypeNo(), startRecord,
                                    stopRecord);
  }

  while (!ctx->isTestStopped()) {
    HugoOperations ops(*pTab);
    ops.setQuiet();
    CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());

    for (int record = startRecord; record < stopRecord; record++) {
      CHK2(ops.pkWriteRecord(pNdb, record, 1, upval) == 0, ops.getNdbError());
    }
    CHK1(result == NDBT_OK);

    int ret = ops.execute_Commit(pNdb);
    err = ops.getNdbError();
    ops.closeTransaction(pNdb);

    if (ret == 0) break;

    require(err.code != 0);
    CHK2(err.status == NdbError::TemporaryError, err);

    if (abort_on_error) {
      g_info << "Temporary error " << err.code << " during write" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (err.code == 410) break;

    info("write: continue on " << err);
    NdbSleep_MilliSleep(100);
  }

  return result;
}

static int runWriteOK(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;

  const bool write_count_rounds =
      (ctx->getProperty("WRITE_COUNT_ROUNDS", (Uint32)0) != 0);

  info("write: start");
  int loop = 0;
  int upval = 0;

  while (!ctx->isTestStopped()) {
    if (loop % 100 == 0) info("write: loop " << loop);

    NdbError err;
    CHK2(run_write_ops(ctx, step, upval++, err) == 0, err);
    if (ctx->isTestStopped()) {
      break;
    }
    require(err.code == 0 || err.code == 410);
    CHK2(err.code == 0, err);
    NdbSleep_MilliSleep(100);

    loop++;
    if (write_count_rounds) {
      ctx->incProperty("WRITE_ROUNDS");
    }
  }

  return result;
}

static int runWrite410(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  const int loops = ctx->getNumLoops();

  info("write: start");
  int loop = 0;
  int upval = 0;

  while (loop < loops && !ctx->isTestStopped()) {
    info("write: loop " << loop);

    while (!ctx->isTestStopped()) {
      NdbError err;
      CHK2(run_write_ops(ctx, step, upval++, err) == 0, err);
      if (err.code != 0) {
        require(err.code == 410);
        info("write: setting 410");
        set_err410(ctx, 1);
        break;
      }
      NdbSleep_MilliSleep(100);
    }

    while (1) {
      int v = get_err410(ctx);
      if (v != 2) {
        require(v == 1);
      } else {
        info("write: longtrans rollback seen");
        break;
      }
      NdbSleep_SecSleep(1);
    }

    while (!ctx->isTestStopped()) {
      NdbError err;
      CHK2(run_write_ops(ctx, step, upval++, err) == 0, err);
      if (err.code == 0) {
        info("write: clearing 410");
        set_err410(ctx, 0);
        break;
      }
      require(err.code == 410);
      NdbSleep_MilliSleep(100);
    }

    loop++;
  }

  info("write: stop test");
  ctx->stopTest();
  return result;
}

struct OpLat {
  const int m_op;
  const int m_repeat;
  // 2: 0-410 off 1-410 on
  // 3: 0-op ok 1-op 410 2-op other temp error
  NDBT_Stats m_lat[2][3];
  OpLat(int op, int repeat) : m_op(op), m_repeat(repeat) {}
};

static void run_latency_report(const OpLat *oplist, int opcnt) {
  for (int i = 0; i < opcnt; i++) {
    const OpLat &oplat = oplist[i];
    printf("optype: %d\n", oplat.m_op);
    for (int i0 = 0; i0 < 2; i0++) {
      printf("410 off/on: %d\n", i0);
      printf("op status ok / 410 / other temp error:\n");
      for (int i1 = 0; i1 < 3; i1++) {
        const NDBT_Stats &lat = oplat.m_lat[i0][i1];
        printf("count: %d", lat.getCount());
        if (lat.getCount() > 0) {
          printf(" mean: %.2f min: %.2f max: %.2f stddev: %.2f", lat.getMean(),
                 lat.getMin(), lat.getMax(), lat.getStddev());
        }
        printf("\n");
      }
    }
  }
}

static int run_latency_ops(NDBT_Context *ctx, NDBT_Step *step, OpLat &oplat,
                           int upval, NdbError &err) {
  Ndb *pNdb = GETNDB(step);
  const int records = ctx->getNumRecords();
  int result = NDBT_OK;

  const NdbDictionary::Table *pTab = g_tabptr[2];
  require(pTab != 0);

  int record = 0;
  while (record < records && !ctx->isTestStopped()) {
    HugoOperations ops(*pTab);
    ops.setQuiet();

    const NDB_TICKS timer_start = NdbTick_getCurrentTicks();

    CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());

    switch (oplat.m_op) {
      case NdbOperation::InsertRequest:
        CHK2(ops.pkInsertRecord(pNdb, record, 1, upval) == 0,
             ops.getNdbError());
        break;
      case NdbOperation::UpdateRequest:
        CHK2(ops.pkUpdateRecord(pNdb, record, 1, upval) == 0,
             ops.getNdbError());
        break;
      case NdbOperation::ReadRequest:
        CHK2(ops.pkReadRecord(pNdb, record, 1) == 0, ops.getNdbError());
        break;
      case NdbOperation::DeleteRequest:
        CHK2(ops.pkDeleteRecord(pNdb, record, 1) == 0, ops.getNdbError());
        break;
      default:
        require(false);
        break;
    }
    CHK2(result == NDBT_OK,
         "latency: ndbapi error at op " << oplat.m_op << " record" << record);

    int ret = ops.execute_Commit(pNdb);
    err = ops.getNdbError();
    ops.closeTransaction(pNdb);

    if (ret != 0) {
      require(err.code != 0);
      CHK2(err.status == NdbError::TemporaryError, err);
    }

    const NDB_TICKS timer_stop = NdbTick_getCurrentTicks();
    Uint64 tt = NdbTick_Elapsed(timer_start, timer_stop).microSec();
    require(tt > 0);
    double td = (double)tt;
    int i0 = get_err410(ctx);
    int i1 = (ret == 0 ? 0 : (err.code == 410 ? 1 : 2));
    NDBT_Stats &lat = oplat.m_lat[i0][i1];
    lat.addObservation(td);

    if (ret == 0) record++;
  }

  return result;
}

static int runLatency(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;

  info("latency: start");
  const int opcnt = 4;
  OpLat oplist[opcnt] = {OpLat(NdbOperation::InsertRequest, 1),
                         OpLat(NdbOperation::UpdateRequest, 10),
                         OpLat(NdbOperation::ReadRequest, 5),
                         OpLat(NdbOperation::DeleteRequest, 1)};

  int loop = 0;
  int upval = 0;

  while (!ctx->isTestStopped()) {
    info("latency: loop " << loop);
    for (int i = 0; i < opcnt && !ctx->isTestStopped(); i++) {
      OpLat &oplat = oplist[i];
      NdbError err;
      for (int j = 0; j < oplat.m_repeat && !ctx->isTestStopped(); j++) {
        CHK2(run_latency_ops(ctx, step, oplat, upval, err) == 0, err);
        upval++;
      }
      CHK1(result == NDBT_OK);
    }
    loop++;
  }

  run_latency_report(oplist, opcnt);
  return result;
}

// gnu bitmap madness
#undef reset
#undef isset

struct LogPos {
  int m_fileno;
  int m_mb;
  int m_pos;  // absolute mb
  friend NdbOut &operator<<(NdbOut &, const LogPos &);
};

NdbOut &operator<<(NdbOut &out, const LogPos &pos) {
  out << pos.m_fileno;
  out << "." << pos.m_mb;
  out << "-" << pos.m_pos;
  return out;
}

struct LogPart {
  int m_partno;  // for print
  bool m_set;
  int m_files;     // redo files
  int m_filesize;  // mb
  int m_total;     // m_files * m_filesize
  int m_free;      // mb
  int m_used;      // mb
  LogPos m_head;
  LogPos m_tail;
  int m_fileused;
  void reset() { m_set = false; }
  bool isset() { return m_set; }
  friend NdbOut &operator<<(NdbOut &, const LogPart &);
};

NdbOut &operator<<(NdbOut &out, const LogPart &lp) {
  out << "part " << lp.m_partno << ":";
  out << " files=" << lp.m_files;
  out << " filesize=" << lp.m_filesize;
  out << " total=" << lp.m_total;
  out << " free=" << lp.m_free;
  out << " head: " << lp.m_head;
  out << " tail: " << lp.m_tail;
  out << " fileused=" << lp.m_fileused;
  return out;
}

struct LogNode {
  int m_nodeid;
  LogPart m_logpart[4];
  int m_files;  // from LogPart (must be same for all)
  int m_filesize;
  int m_minfds;  // min and max FDs in page 0
  int m_maxfds;  // LQH uses max FDs by default
  void reset() {
    for (int i = 0; i < 4; i++) {
      m_logpart[i].m_partno = i;
      m_logpart[i].reset();
    }
  }
  bool isset() {
    for (int i = 0; i < 4; i++)
      if (!m_logpart[i].isset()) return false;
    return true;
  }
};

struct LogInfo {
  int m_nodes;
  LogNode *m_lognode;
  int m_files;  // from LogNode (config is same for all in these tests)
  int m_filesize;
  int m_minfds;
  int m_maxfds;
  LogInfo(int nodes) {
    m_nodes = nodes;
    m_lognode = new LogNode[nodes];
    reset();
  }
  ~LogInfo() {
    m_nodes = 0;
    delete[] m_lognode;
  }
  void reset() {
    for (int n = 0; n < m_nodes; n++) m_lognode[n].reset();
  }
  bool isset() {
    for (int n = 0; n < m_nodes; n++)
      if (!m_lognode[n].isset()) return false;
    return true;
  }
  LogNode *findnode(int nodeid) const {
    for (int n = 0; n < m_nodes; n++) {
      if (m_lognode[n].m_nodeid == nodeid) return &m_lognode[n];
    }
    return 0;
  }
  void copyto(LogInfo &li2) const {
    require(m_nodes == li2.m_nodes);
    for (int n = 0; n < m_nodes; n++) {
      const LogNode &ln1 = m_lognode[n];
      LogNode &ln2 = li2.m_lognode[n];
      ln2 = ln1;
    }
  }
};

static int get_nodestatus(NdbMgmHandle h, LogInfo &li) {
  int result = 0;
  ndb_mgm_cluster_state *cs = 0;

  do {
    require(h != 0);
    CHK2((cs = ndb_mgm_get_status(h)) != 0, ndb_mgm_get_latest_error_msg(h));
    int n = 0;
    for (int i = 0; i < cs->no_of_nodes; i++) {
      ndb_mgm_node_state &ns = cs->node_states[i];
      if (ns.node_type == NDB_MGM_NODE_TYPE_NDB) {
        // called only when all started
        CHK1(ns.node_status == NDB_MGM_NODE_STATUS_STARTED);
        CHK1(n < li.m_nodes);

        LogNode &ln = li.m_lognode[n];
        ln.m_nodeid = ns.node_id;
        info("node " << n << ": " << ln.m_nodeid);
        n++;
      }
      CHK1(result == 0);
    }
    CHK1(n == li.m_nodes);
  } while (0);

  free(cs);
  info("get_nodestatus result=" << result);
  return result;
}

static int get_redostatus(NdbMgmHandle h, LogInfo &li) {
  int result = 0;

  do {
    li.reset();
    require(h != 0);

    int filter[] = {15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0};
    NdbLogEventHandle evh = 0;
    CHK2((evh = ndb_mgm_create_logevent_handle(h, filter)) != 0,
         ndb_mgm_get_latest_error_msg(h));

    for (int n = 0; n < li.m_nodes; n++) {
      int dump[] = {2399};
      const LogNode &ln = li.m_lognode[n];
      struct ndb_mgm_reply reply;
      CHK2(ndb_mgm_dump_state(h, ln.m_nodeid, dump, 1, &reply) == 0,
           ndb_mgm_get_latest_error_msg(h));
    }
    CHK1(result == 0);

    int maxcnt = 4 * li.m_nodes;
    int rescnt = 0;
    time_t start = time(0);
    int maxwait = 5;

    while (rescnt < maxcnt && time(0) < start + maxwait) {
      while (1) {
        int res;
        ndb_logevent ev;
        int msec = 100;
        CHK2((res = ndb_logevent_get_next(evh, &ev, msec)) >= 0,
             ndb_mgm_get_latest_error_msg(h));
        if (res == 0) break;
        if (ev.type != NDB_LE_RedoStatus) continue;

        LogNode *lnptr = 0;
        CHK2((lnptr = li.findnode(ev.source_nodeid)) != 0,
             "unknown nodeid " << ev.source_nodeid);
        LogNode &ln = *lnptr;

        const ndb_logevent_RedoStatus &rs = ev.RedoStatus;
        CHK1(rs.log_part < 4);
        LogPart &lp = ln.m_logpart[rs.log_part];

        info("RedoStatus from node " << ev.source_nodeid << " log part "
                                     << rs.log_part);

        CHK1(!lp.m_set);
        LogPos &head = lp.m_head;
        LogPos &tail = lp.m_tail;
        lp.m_files = rs.no_logfiles;
        lp.m_filesize = rs.logfilesize;
        head.m_fileno = rs.head_file_no;
        head.m_mb = rs.head_mbyte;
        head.m_pos = head.m_fileno * lp.m_filesize + head.m_mb;
        tail.m_fileno = rs.tail_file_no;
        tail.m_mb = rs.tail_mbyte;
        tail.m_pos = tail.m_fileno * lp.m_filesize + tail.m_mb;
        CHK1(rs.total_hi == 0 && rs.total_lo < (1u << 31));
        lp.m_total = rs.total_lo;
        CHK1(rs.free_hi == 0 && rs.free_lo < (1u << 31));
        lp.m_free = rs.free_lo;
        lp.m_used = lp.m_total - lp.m_free;

        // set number of files used
        if (tail.m_fileno < head.m_fileno) {
          lp.m_fileused = head.m_fileno - tail.m_fileno + 1;
        } else if (tail.m_fileno > head.m_fileno) {
          lp.m_fileused = lp.m_files - (tail.m_fileno - head.m_fileno - 1);
        } else if (tail.m_pos < head.m_pos) {
          lp.m_fileused = 1;
        } else if (tail.m_pos > head.m_pos) {
          lp.m_fileused = lp.m_files;
        } else {
          lp.m_fileused = 0;
        }

        // sanity checks
        {
          CHK2(lp.m_total == lp.m_files * lp.m_filesize, lp);
          CHK2(head.m_fileno < lp.m_files, lp);
          CHK2(head.m_mb < lp.m_filesize, lp);
          require(head.m_pos < lp.m_total);
          CHK2(tail.m_fileno < lp.m_files, lp);
          CHK2(tail.m_mb < lp.m_filesize, lp);
          require(tail.m_pos < lp.m_total);
          CHK2(lp.m_free <= lp.m_total, lp);
          if (tail.m_pos <= head.m_pos) {
            CHK2(lp.m_free == lp.m_total - (head.m_pos - tail.m_pos), lp);
          } else {
            CHK2(lp.m_free == tail.m_pos - head.m_pos, lp);
          }
        }
        lp.m_set = true;
        // info("node " << ln.m_nodeid << ": " << lp);

        rescnt++;
      }
      CHK1(result == 0);
    }
    CHK1(result == 0);
    CHK2(rescnt == maxcnt, "got events (after " << Int64(time(0) - start)
                                                << "s of " << maxwait << "s) "
                                                << rescnt << " != " << maxcnt);
    require(li.isset());  // already implied by counts

    for (int n = 0; n < li.m_nodes; n++) {
      LogNode &ln = li.m_lognode[n];
      for (int i = 0; i < 4; i++) {
        LogPart &lp = ln.m_logpart[i];
        if (i == 0) {
          ln.m_files = lp.m_files;
          ln.m_filesize = lp.m_filesize;
          CHK1(ln.m_files >= 3 && ln.m_filesize >= 4);

          // see Dblqh::execREAD_CONFIG_REQ()
          ln.m_minfds = 2;
          ln.m_maxfds = (8192 - 32 - 128) / (3 * ln.m_filesize);
          if (ln.m_maxfds > 40) ln.m_maxfds = 40;
          CHK1(ln.m_minfds <= ln.m_maxfds);
        } else {
          CHK1(ln.m_files == lp.m_files && ln.m_filesize == lp.m_filesize);
        }
      }

      if (n == 0) {
        li.m_files = ln.m_files;
        li.m_filesize = ln.m_filesize;
        li.m_minfds = ln.m_minfds;
        li.m_maxfds = ln.m_maxfds;
        require(li.m_files > 0 && li.m_filesize > 0);
        require(li.m_minfds <= li.m_maxfds);
      } else {
        CHK1(li.m_files == ln.m_files && li.m_filesize == ln.m_filesize);
        require(li.m_minfds == ln.m_minfds && li.m_maxfds == ln.m_maxfds);
      }

      CHK1(result == 0);
    }
    CHK1(result == 0);

    ndb_mgm_destroy_logevent_handle(&evh);
  } while (0);

  info("get_redostatus result=" << result);
  return result;
}

// get node with max redo files used in some part

struct LogUsed {
  int m_nodeidx;
  int m_nodeid;
  int m_partno;
  int m_used;  // mb
  LogPos m_head;
  LogPos m_tail;
  int m_fileused;
  int m_rand;  // randomize node to restart if file usage is same
  friend NdbOut &operator<<(NdbOut &, const LogUsed &);
};

NdbOut &operator<<(NdbOut &out, const LogUsed &lu) {
  out << "n=" << lu.m_nodeid;
  out << " p=" << lu.m_partno;
  out << " u=" << lu.m_used;
  out << " h=" << lu.m_head;
  out << " t=" << lu.m_tail;
  out << " f=" << lu.m_fileused;
  return out;
}

static int cmp_logused(const void *a1, const void *a2) {
  const LogUsed &lu1 = *(const LogUsed *)a1;
  const LogUsed &lu2 = *(const LogUsed *)a2;
  int k = lu1.m_fileused - lu2.m_fileused;
  if (k != 0) {
    // sorting by larger file usage
    return (-1) * k;
  }
  return lu1.m_rand - lu2.m_rand;
}

struct LogMax {
  int m_nodes;
  LogUsed *m_logused;
  LogMax(int nodes) {
    m_nodes = nodes;
    m_logused = new LogUsed[nodes];
  }
  ~LogMax() {
    m_nodes = 0;
    delete[] m_logused;
  }
};

static void get_redoused(const LogInfo &li, LogMax &lx) {
  require(li.m_nodes == lx.m_nodes);
  for (int n = 0; n < li.m_nodes; n++) {
    const LogNode &ln = li.m_lognode[n];
    LogUsed &lu = lx.m_logused[n];
    lu.m_used = -1;
    for (int i = 0; i < 4; i++) {
      const LogPart &lp = ln.m_logpart[i];
      if (lu.m_used < lp.m_used) {
        lu.m_nodeidx = n;
        lu.m_nodeid = ln.m_nodeid;
        lu.m_partno = i;
        lu.m_used = lp.m_used;
        lu.m_head = lp.m_head;
        lu.m_tail = lp.m_tail;
        lu.m_fileused = lp.m_fileused;
        lu.m_rand = myRandom48(100);
      }
    }
  }
  qsort(lx.m_logused, lx.m_nodes, sizeof(LogUsed), cmp_logused);
  for (int n = 0; n + 1 < li.m_nodes; n++) {
    const LogUsed &lu1 = lx.m_logused[n];
    const LogUsed &lu2 = lx.m_logused[n + 1];
    require(lu1.m_fileused >= lu2.m_fileused);
  }
}

struct LogDiff {
  bool m_tailmove;  // all tails since all redo parts are used
};

static void get_redodiff(const LogInfo &li1, const LogInfo &li2, LogDiff &ld) {
  require(li1.m_nodes == li2.m_nodes);
  ld.m_tailmove = true;
  for (int i = 0; i < li1.m_nodes; i++) {
    LogNode &ln1 = li1.m_lognode[i];
    LogNode &ln2 = li2.m_lognode[i];
    for (int j = 0; j < 4; j++) {
      LogPart &lp1 = ln1.m_logpart[j];
      LogPart &lp2 = ln2.m_logpart[j];
      if (lp1.m_tail.m_pos == lp2.m_tail.m_pos) {
        ld.m_tailmove = false;
      }
    }
  }
}

static int runRestartOK(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  const int loops = ctx->getNumLoops();
  NdbRestarter restarter;

  info("restart01: start");
  int nodes = restarter.getNumDbNodes();
  require(nodes >= 1);
  info("restart: nodes " << nodes);

  if (nodes == 1) {
    info("restart01: need at least 2 nodes");
    return result;
  }

  int nodeidx = myRandom48(nodes);
  int nodeid = restarter.getDbNodeId(nodeidx);
  info("restart01: using nodeid " << nodeid);

  LogInfo logInfo(nodes);

  int loop = 0;
  while (loop < loops && !ctx->isTestStopped()) {
    info("restart01: loop " << loop);
    CHK1(get_nodestatus(restarter.handle, logInfo) == 0);

    bool fi = false;
    bool fn = false;
    bool fa = false;
    info("restart01: restart nodeid " << nodeid);
    CHK1(restarter.restartOneDbNode(nodeid, fi, fn, fa) == 0);
    CHK1(restarter.waitClusterStarted() == 0);
    info("restart01: cluster up again");

    // let write run until redo wraps (no check yet)
    NdbSleep_SecSleep(300);
    loop++;
  }

  info("restart01: stop test");
  ctx->stopTest();
  return result;
}

#define g_SETFDS "SETFDS"

static int run_write_ops(NDBT_Context *ctx, NDBT_Step *step, int cnt,
                         int &upval, NdbError &err) {
  int result = NDBT_OK;

  for (int i = 0; i < cnt && !ctx->isTestStopped(); i++) {
    CHK2(run_write_ops(ctx, step, upval++, err) == 0, err);
    if (err.code != 0) {
      require(err.code == 410);
      break;
    }
  }

  return result;
}

static int get_newfds(const LogInfo &li) {
  require(li.m_files >= 3);
  int newfds = li.m_files - 1;
  require(newfds >= li.m_minfds);
  // twice to prefer smaller
  newfds = li.m_minfds + myRandom48(newfds - li.m_minfds + 1);
  newfds = li.m_minfds + myRandom48(newfds - li.m_minfds + 1);
  return newfds;
}

static int get_limfds(const LogInfo &li, int newfds) {
  int off = li.m_files - newfds;
  require(off > 0);
  off = myRandom48(off + 1);
  off = myRandom48(off + 1);
  return newfds + off;
}

static int run_restart(NDBT_Context *ctx, NDBT_Step *step, int nodeid,
                       bool fi) {
  int result = NDBT_OK;
  int setfds = ctx->getProperty(g_SETFDS, (Uint32)0xff);
  require(setfds != 0xff);
  int dump[2] = {2396, setfds};
  NdbRestarter restarter;
  info("run_restart: nodeid=" << nodeid << " initial=" << fi
                              << " setfds=" << setfds);

  /*
   * When starting non-initial the node(s) have already some setfds
   * but it is lost on restart.  We must dump the same setfds again.
   */
  do {
    bool fn = true;
    bool fa = false;
    if (nodeid == 0) {
      info("run_restart: restart all nostart");
      CHK1(restarter.restartAll(fi, fn, fa) == 0);
      info("run_restart: wait nostart");
      CHK1(restarter.waitClusterNoStart() == 0);
      info("run_restart: dump " << dump[0] << " " << dump[1]);
      CHK1(restarter.dumpStateAllNodes(dump, 2) == 0);
      info("run_restart: start all");
      CHK1(restarter.startAll() == 0);
    } else {
      info("run_restart: restart node nostart");
      CHK1(restarter.restartOneDbNode(nodeid, fi, fn, fa) == 0);
      info("run_restart: wait nostart");
      CHK1(restarter.waitNodesNoStart(&nodeid, 1) == 0);
      info("run_restart: dump " << dump[0] << " " << dump[1]);
      CHK1(restarter.dumpStateAllNodes(dump, 2) == 0);
      info("run_restart: start all");
      CHK1(restarter.startAll() == 0);
    }
    info("run_restart: wait started");
    CHK1(restarter.waitClusterStarted() == 0);
    info("run_restart: started");
  } while (0);

  info("run_restart: result=" << result);
  return result;
}

static int run_start_lcp(NdbRestarter &restarter) {
  int result = NDBT_OK;
  int dump[1] = {7099};
  do {
    CHK1(restarter.dumpStateAllNodes(dump, 1) == 0);
  } while (0);
  info("run_start_lcp: result=" << result);
  return result;
}

/*
 * Start long trans to freeze log tail.  Run writes until over
 * FDs stored in zero-pages (may hit 410).  Run restart (which
 * aborts long trans) and verify log tail moves (must not hit 410).
 * At start and every 5 loops do initial restart and DUMP to
 * change number of FDs stored to a random number between 2
 * (minimum) and number of redo log files minus 1.
 */

static int runRestartFD(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  int result = NDBT_OK;
  const int loops = ctx->getNumLoops();
  const bool srflag = ctx->getProperty("SRFLAG", (Uint32)0);
  NdbRestarter restarter;

  info("restart: start srflag=" << srflag);
  int nodes = restarter.getNumDbNodes();
  require(nodes >= 1);
  info("restart: nodes " << nodes);

  if (nodes == 1 && !srflag) {
    info("restart: need at least 2 nodes");
    return result;
  }

  LogInfo logInfo(nodes);
  LogInfo logInfo2(nodes);
  LogMax logMax(nodes);
  LogDiff logDiff;

  const NdbDictionary::Table *pTab = 0;

  int upval = 0;
  int loop = 0;
  int newfds = 0;
  int limfds = 0;
  while (loop < loops && !ctx->isTestStopped()) {
    info("restart: loop " << loop);
    if (loop % 5 == 0) {
      CHK1(get_nodestatus(restarter.handle, logInfo) == 0);
      CHK1(get_redostatus(restarter.handle, logInfo) == 0);

      // set new cmaxLogFilesInPageZero in all LQH nodes
      newfds = get_newfds(logInfo);
      ctx->setProperty(g_SETFDS, (Uint32)newfds);
      bool nodeid = 0;  // all nodes
      bool fi = true;   // initial start
      CHK1(run_restart(ctx, step, nodeid, fi) == 0);

      CHK1(runCreate(ctx, step) == 0);
      pTab = g_tabptr[0];
      require(pTab != 0);
    }

    // start long trans
    HugoOperations ops(*pTab);
    ops.setQuiet();
    CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());
    for (int i = 0; i < 100; i++) {
      CHK2(ops.pkInsertRecord(pNdb, i, 1, 0) == 0, ops.getNdbError());
    }
    CHK2(ops.execute_NoCommit(pNdb) == 0, ops.getNdbError());

    // randomize load1 limit a bit upwards
    limfds = get_limfds(logInfo, newfds);
    // may be up to logInfo.m_files and then hit 410
    require(newfds <= limfds && limfds <= logInfo.m_files);
    info("restart: newfds=" << newfds << " limfds=" << limfds);

    // start load1 loop
    info("restart: load1");
    while (!ctx->isTestStopped()) {
      info("restart: load1 at " << upval);
      NdbError err;
      int cnt = 100 + myRandom48(100);
      CHK1(run_write_ops(ctx, step, cnt, upval, err) == 0);

      CHK1(get_redostatus(restarter.handle, logInfo) == 0);
      get_redoused(logInfo, logMax);
      info("restart: load1 max: " << logMax.m_logused[0]);
      info("restart: load1 min: " << logMax.m_logused[nodes - 1]);

      if (err.code != 0) {
        require(err.code == 410);
        info("restart: break load1 on 410");
        break;
      }

      int fileused = logMax.m_logused[0].m_fileused;
      if (fileused > limfds) {
        info("restart: break load1 on file usage > FDs");
        break;
      }
    }
    CHK1(result == NDBT_OK);

    // restart
    if (srflag) {
      int nodeid = 0;
      int fi = false;
      CHK1(run_restart(ctx, step, nodeid, fi) == 0);
    } else {
      int nodeid = logMax.m_logused[0].m_nodeid;
      int fi = false;
      CHK1(run_restart(ctx, step, nodeid, fi) == 0);
    }

    // start load2 loop
    info("restart: load2");
    CHK1(get_redostatus(restarter.handle, logInfo) == 0);
    logInfo.copyto(logInfo2);

    // should be fast but allow for slow machines
    int retry2 = 0;
    while (!ctx->isTestStopped()) {
      info("restart: load2 at " << upval);
      NdbError err;
      int cnt = 100 + myRandom48(100);
      CHK1(run_write_ops(ctx, step, cnt, upval, err) == 0);

      CHK1(get_redostatus(restarter.handle, logInfo2) == 0);
      get_redoused(logInfo2, logMax);
      info("restart: load2 max: " << logMax.m_logused[0]);
      info("restart: load2 min: " << logMax.m_logused[nodes - 1]);

      require(err.code == 0 || err.code == 410);
      CHK2(retry2 < 60 || err.code == 0, err);

      get_redodiff(logInfo, logInfo2, logDiff);
      if (logDiff.m_tailmove) {
        info("restart: break load2");
        break;
      }

      info("restart: retry2=" << retry2);
      if (retry2 % 5 == 0) {
        CHK1(run_start_lcp(restarter) == 0);
        NdbSleep_MilliSleep(1000);
      }
      retry2++;
    }
    CHK1(result == NDBT_OK);

    NdbSleep_SecSleep(1 + myRandom48(10));
    loop++;
  }

  info("restart: stop test");
  ctx->stopTest();
  return result;
}

static int runResetFD(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int oldfds = ctx->getProperty(g_SETFDS, (Uint32)-1);
  do {
    if (oldfds == -1) {
      // never changed (some step failed)
      break;
    }
    ctx->setProperty(g_SETFDS, (Uint32)0);
    CHK1(run_restart(ctx, step, 0, true) == 0);
  } while (0);
  return result;
}

static int resizeRedoLog(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_FAILED;
  Config conf;
  NdbRestarter restarter;
  Uint32 noOfLogFiles = ctx->getProperty("REDOLOGCOUNT", (Uint32)4);
  Uint32 logFileSize =
      ctx->getProperty("REDOLOGSIZE", (Uint32)64 * 1024 * 1024);
  Uint32 LCPinterval = ctx->getProperty("LCPINTERVAL", (Uint32)20);
  Uint32 defaultNoOfLogFiles = 0, defaultLogFileSize = 0;
  Uint32 defaultLCPinterval = 0;

  do {
    NdbMgmd mgmd;
    mgmd.use_tls(opt_tls_search_path, opt_mgm_tls);
    if (!mgmd.connect()) {
      g_err << "Failed to connect to ndb_mgmd." << endl;
      break;
    }
    if (!mgmd.get_config(conf)) {
      g_err << "Failed to get config from ndb_mgmd." << endl;
      break;
    }

    g_err << "Setting NoOfFragmentLogFiles = " << noOfLogFiles
          << " FragmentLogFileSize = " << logFileSize << " TimeBetweenLCP "
          << LCPinterval << endl;
    ConfigValues::Iterator iter(conf.m_configuration->m_config_values);
    for (int idx = 0; iter.openSection(CFG_SECTION_NODE, idx); idx++) {
      Uint32 oldValue;
      if (iter.get(CFG_DB_NO_REDOLOG_FILES, &oldValue)) {
        iter.set(CFG_DB_NO_REDOLOG_FILES, noOfLogFiles);
        if (defaultNoOfLogFiles == 0) {
          defaultNoOfLogFiles = oldValue;
        } else if (oldValue != defaultNoOfLogFiles) {
          g_err << "NoOfFragmentLogFiles is not consistent across nodes"
                << endl;
          break;
        }
      }
      if (iter.get(CFG_DB_REDOLOG_FILE_SIZE, &oldValue)) {
        iter.set(CFG_DB_REDOLOG_FILE_SIZE, logFileSize);
        if (defaultLogFileSize == 0) {
          defaultLogFileSize = oldValue;
        } else if (oldValue != defaultLogFileSize) {
          g_err << "FragmentLogFileSize is not consistent across nodes" << endl;
          break;
        }
      }
      if (iter.get(CFG_DB_LCP_INTERVAL, &oldValue)) {
        iter.set(CFG_DB_LCP_INTERVAL, LCPinterval);
        if (defaultLCPinterval == 0) {
          defaultLCPinterval = oldValue;
        } else if (oldValue != defaultLCPinterval) {
          g_err << "defaultLCPinterval is not consistent across nodes" << endl;
          break;
        }
      }
      iter.closeSection();
    }

    // Save old config values
    ctx->setProperty("REDOLOGCOUNT", (Uint32)defaultNoOfLogFiles);
    ctx->setProperty("REDOLOGSIZE", (Uint32)defaultLogFileSize);
    ctx->setProperty("LCPINTERVAL", (Uint32)defaultLCPinterval);

    if (!mgmd.set_config(conf)) {
      g_err << "Failed to set config in ndb_mgmd." << endl;
      break;
    }

    g_err << "Restarting nodes to apply config change..." << endl;
    NdbSleep_SecSleep(3);  // Give MGM server time to restart
    if (restarter.restartAll(true)) {
      g_err << "Failed to restart node." << endl;
      break;
    }
    if (restarter.waitClusterStarted(120) != 0) {
      g_err << "Failed waiting for node started." << endl;
      break;
    }
    g_err << "Nodes restarted with new config" << endl;
    result = NDBT_OK;
  } while (0);
  return result;
}

static int start_open_transaction(NDBT_Context *ctx, NDBT_Step *step,
                                  HugoOperations **ops) {
  /**
   * Ensure we don't use the same record for the open transaction as for
   * the ones filling up the REDO log. In that case we get into a deadlock,
   * we solve this by using a different table for the pending transaction.
   */
  const NdbDictionary::Table *pTab = g_tabptr[0];

  g_info << "Starting a write and leaving it open so the pending "
         << "COMMIT indefinitely delays redo log trimming" << pTab << endl;

  *ops = new HugoOperations(*pTab);
  CHK3((*ops) != NULL, "Could not create new HugoOperations");

  (*ops)->setQuiet();

  Ndb *pNdb = GETNDB(step);
  CHK3((*ops)->startTransaction(pNdb) == 0,
       "Failed to start transaction: error ");
  int upval = 0;
  CHK3((*ops)->pkWriteRecord(pNdb, 0, 1, upval++) == 0, (*ops)->getNdbError());
  CHK3((*ops)->execute_NoCommit(pNdb) == 0,
       "Error: failed to execute NoCommit");

  return NDBT_OK;
}

static int runWriteWithRedoFull(NDBT_Context *ctx, NDBT_Step *step) {
  int upval = 0;
  NdbRestarter restarter;
  Ndb *pNdb = GETNDB(step);

  // Block redo logpart being trimmed by holding a transaction open
  HugoOperations *ops = NULL;
  start_open_transaction(ctx, step, &ops);

  g_err << "Starting PK insert load..." << endl;
  int loop = 0;
  int result = NDBT_FAILED;
  while (!ctx->isTestStopped()) {
    if (loop % 100 == 0) {
      info("write: loop " << loop);
    }

    NdbError err;
    run_write_ops(ctx, step, upval++, err, true);
    if (err.code == 410) {
      g_err << "Redo log full, new requests aborted as expected" << endl;
      result = NDBT_OK;
      break;
    } else if (err.code == 266) {
      g_err << "Error; redo log full, but new requests still allowed to queue"
            << endl;
      break;
    } else if (err.code != 0) {
      g_err << "Error: write failed with unexpected error " << err.code << endl;
      break;
    }
    loop++;
  }

  g_err << "Executing pending COMMIT so that redo log can be trimmed..."
        << endl;
  int ret = ops->execute_Commit(pNdb);
  if (ret != 0) {
    g_err << "Error: failed to execute commit" << ops->getNdbError() << endl;
    result = NDBT_FAILED;
  }
  ops->closeTransaction(pNdb);
  return result;
}

/**
 * Run one or more LCP requests when signalled until stop is signalled
 */
int runLCP(NDBT_Context *ctx, NDBT_Step *step) {
  while ((ctx->getProperty("stop_lcp", (Uint32)0)) == 0 &&
         !ctx->isTestStopped()) {
    NdbSleep_MilliSleep(1000);
    // Check whether start lcp is signalled
    Uint32 lcps = 0;
    if ((lcps = ctx->getProperty("start_lcp", (Uint32)0)) == 0) continue;

    // Perform LCP the number of times indicated by 'lcps'
    ctx->setProperty("lcps_done", (Uint32)0);
    NdbRestarter restarter;
    int dump[] = {DumpStateOrd::DihStartLcpImmediately};
    restarter.getNumDbNodes();

    int filter[] = {15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0};
    NdbLogEventHandle handle =
        ndb_mgm_create_logevent_handle(restarter.handle, filter);

    struct ndb_logevent event;

    for (Uint32 i = 0; i < lcps; i++) {
      CHK3(restarter.dumpStateAllNodes(dump, 1) == 0, "Could not start LCP");
      while (ndb_logevent_get_next(handle, &event, 0) >= 0 &&
             event.type != NDB_LE_LocalCheckpointStarted)
        ;
      while (ndb_logevent_get_next(handle, &event, 0) >= 0 &&
             event.type != NDB_LE_LocalCheckpointCompleted)
        ;
    }

    // Signal lcps done
    ctx->setProperty("lcps_done", (Uint32)1);
    ctx->setProperty("start_lcp", (Uint32)0);
  }

  return NDBT_OK;
}

/**
 * If the given logpart_with_maxusage and nodeid are invalid,
 *   return the maximum REDO log usage and the node id and logpart
 *      which is having it. Fails if two distinct logparts
 *      (except primary and backup) have same usage.
 * else return the REDO log usage of the given nodeid and logpart.
 */
static int get_redo_logpart_maxusage(NDBT_Context *ctx, Uint32 &nodeid,
                                     Uint32 &logpart_with_maxusage) {
  NdbInfo ndbinfo(&ctx->m_cluster_connection, "ndbinfo/");
  if (!ndbinfo.init()) {
    g_err << "ndbinfo.init failed" << endl;
    return -1;
  }

  const NdbInfo::Table *table;
  if (ndbinfo.openTable("ndbinfo/logspaces", &table) != 0) {
    g_err << "Failed to openTable(logspaces)" << endl;
    return -1;
  }

  NdbInfoScanOperation *scanOp = NULL;
  if (ndbinfo.createScanOperation(table, &scanOp)) {
    g_err << "No NdbInfoScanOperation" << endl;
    return -1;
  }

  if (scanOp->readTuples() != 0) {
    g_err << "scanOp->readTuples failed" << endl;
    return -1;
  }

  const NdbInfoRecAttr *nodeid_colval = scanOp->getValue("node_id");
  const NdbInfoRecAttr *logtype_colval = scanOp->getValue("log_type");
  const NdbInfoRecAttr *logpart_colval = scanOp->getValue("log_part");
  const NdbInfoRecAttr *total_colval = scanOp->getValue("total");
  const NdbInfoRecAttr *used_colval = scanOp->getValue("used");

  if (scanOp->execute() != 0) {
    g_err << "scanOp->execute failed" << endl;
    return -1;
  }

  // Help variables to trace the max usage and the log part/node id having it
  int max_usage = -1, usage = -1;
  Uint32 max_logpart = UINT32_MAX;
  Uint32 max_node_id = 0;

  while (scanOp->nextResult() == 1) {
    Uint32 node_id = nodeid_colval->u_32_value();
    Uint64 total = total_colval->u_64_value();
    Uint64 used = used_colval->u_64_value();
    Uint32 logtype = logtype_colval->u_32_value();
    Uint32 logpart = logpart_colval->u_32_value();

    /* The result row can be skipped if
     * - it is NOT a redo log data or
     * - it is NOT the row the test has requested to retrieve
     */
    if (logtype != 0 ||  // Not a redo log
        (nodeid != 0 && logpart_with_maxusage != UINT32_MAX &&
         nodeid != node_id && logpart_with_maxusage != logpart)) {
      continue;
    }

    if (total != 0) {
      usage = (int)((100 * used) / total);

      g_info << "nodeid " << node_id << " " << nodeid << " logpart " << logpart
             << " " << logpart_with_maxusage << " usage " << usage << " "
             << max_usage << endl;

      // Requested row is found
      if (node_id == nodeid && logpart == logpart_with_maxusage) {
        g_err << "Row with requested nodeid " << nodeid << " and logpart "
              << logpart << "  is found. Usage " << usage << " used = " << used
              << " total = " << total << endl;
        return usage;
      }

      /* The test blocks one logpart from being trimmed.
       * The following check may become true when LCP races with the load.
       * The probability is less for runCheckLCPStartsAfterSR
       * than for runCheckLCPStartsAfterNR,
       * since the latter calls this method without LCPs performed.
       */
      if (usage > 0 && usage == max_usage && max_logpart != logpart &&
          max_node_id != node_id) {
        g_err << "Two non-peer log parts having same usage is not handled"
              << endl;
        return -1;
      }

      // Find the max usage and the corresponding nodeid/logpart.
      // Primary and backup logparts will be full. Return the
      // usage of the last row retrieved from ndbinfo/logspace.
      if (usage > max_usage) {
        max_usage = usage;
        max_logpart = logpart;
        max_node_id = node_id;
      }
    }
  }
  ndbinfo.releaseScanOperation(scanOp);
  ndbinfo.closeTable(table);

  // Return the results
  logpart_with_maxusage = max_logpart;
  nodeid = max_node_id;

  g_err << "get_redo_logpart_maxusage returns: nodeid " << nodeid << " lp "
        << logpart_with_maxusage << " usage " << max_usage << endl;

  if (max_usage <= 0)
    g_err << " The test could not fill the redo log. Redo log usage : usage "
          << usage << " max usage " << max_usage << endl;

  return max_usage;
}

static int redologpart_is_trimmed(NDBT_Context *ctx, int usage_before,
                                  Uint32 full_logpart, Uint32 nodeid) {
  // Check whether the redo log is trimmed after system or node restart.
  // Wait max 2/3 of max LCP_INTERVAL (20) seconds for an lcp to
  // trim the logpart that was full. Slow machines may need more time.
  int retries = 20;
  int usage_after = -1;
  do {
    NdbSleep_MilliSleep(1000);
    usage_after = get_redo_logpart_maxusage(ctx, nodeid, full_logpart);
    CHK3(usage_after != -1, "Could not retrieve redo log usage");
    g_info << "Retrying : Usage before : " << usage_before
           << " Usage after : " << usage_after << " Retries " << 20 - retries
           << endl;
  }  // while (retries-- > 0 && usage_after >= usage_before);
  while (retries-- > 0 && usage_after > 0);

  if (usage_after > 0) {
    g_err << "Redo log is not trimmed " << 20 - retries
          << " seconds after restart. "
          << " Usage before : " << usage_before
          << " Usage after : " << usage_after << " logpart " << full_logpart
          << " nodeid " << nodeid << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

/**
 * Test to see if lcp is started after an SR and some space
 * from an almost-filled redo log part is released :
 * - Change the config to disable change-based LCP start
 *   by timeBetweenLocalCheckpoints = 31(max, default 20)
 * - Start a write and not commit in order to prevent corresponding
 *     redo logpart from being trimmed. Other log parts are free to be
 *     trimmed as normal.
 * - Fill until some of the redo logpart, most probably the one containing
 *     the redo log for the above open transaction, to become almost full.
 * - Perform 3 LCPs while loading further writes.
 * - Get the <nodeid, logpart> pair that has max usage.
 *     Assumption here is that there will be only one logpart
 *    (primary and backup) that will have the
 *     maximum and others are trimmed by LCPs. If more than one
 *     logparts that will have the same usage, the test will fail.
 * - Start an SR.
 * - After completing the SR, check the redo log usage of
 *   the logpart that went full.
 * - If the usage is not reduced within 2/3 of the LCP_INTERVAL seconds,
 *     (approximates to 20 seconds) the test will fail.
 */
static int runCheckLCPStartsAfterSR(NDBT_Context *ctx, NDBT_Step *step) {
  // Block redo logpart being trimmed by holding a transaction open
  HugoOperations *ops = NULL;
  start_open_transaction(ctx, step, &ops);

  g_info << "Starting normal load and fill some logpart" << endl;

  bool lcp_started = false;
  int upval = 0;
  while (ctx->getProperty("lcps_done", (Uint32)0) != 1 &&
         !ctx->isTestStopped()) {
    NdbError err;
    run_write_ops(ctx, step, upval++, err, true);

    // When some logpart is getting full, continue with the load
    // in order to fill more of it (to its maximum)
    // while performing 3 LCPs
    if (err.code == 410 && !lcp_started) {
      lcp_started = true;
      ctx->setProperty("start_lcp", (Uint32)3);
      g_info << "Starting lcp" << endl;
    }
  }

  if (ctx->isTestStopped()) return NDBT_FAILED;

  // Perform one more checkpoint
  ctx->setProperty("start_lcp", (Uint32)1);

  ctx->setProperty("stop_lcp", (Uint32)1);  // stop runLCP()

  // Find the max redo log usage and the corresponding logpart and nodeid
  int usage_before_SR = -1;
  Uint32 full_logpart = UINT32_MAX;
  Uint32 nodeid = 0;

  usage_before_SR = get_redo_logpart_maxusage(ctx, nodeid, full_logpart);
  CHK3(full_logpart != UINT32_MAX, "No logpart became full");
  CHK3(nodeid != 0, "No nodeid found with almost full logpart");
  CHK3(usage_before_SR > 0, "Redo log usage <= 0");

  NdbRestarter restarter;
  // Perform a system restart
  CHK3(restarter.restartAll(false, true, true) == 0,
       "Starting all nodes failed");
  g_err << "Wait until all nodes are stopped" << endl;
  CHK3(restarter.waitClusterNoStart() == 0,
       "Nodes have not reached NoStart state");
  g_err << "Starting all nodes" << endl;
  CHK3(restarter.startAll() == 0, "Starting all nodes failed");
  CHK3(restarter.waitClusterStarted() == 0, "Cluster has not started");

  // Check whether the full redo log part has been trimmed
  CHK3((redologpart_is_trimmed(ctx, usage_before_SR, full_logpart, nodeid)) ==
           NDBT_OK,
       "Check for redolog trimmed failed");
  return NDBT_OK;
}

/**
 * Test to see if lcp is started after an NR and some space
 * from an almost-filled redo log part is released :
 * - Change the config to disable change-based LCP start
 *   by timeBetweenLocalCheckpoints = 31(max, default 20).
 * - Start a write and not commit in order to prevent corresponding
 *     redo logpart from being trimmed. Other log parts are free to be
 *     trimmed as normal.
 * - Perform 1 LCP.
 * - Generate more writes and perform an LCP.
 * - Fill until some of the redo logparts, most probably the one containing
 *     the redo log for the above open transaction, to become almost full.
 *   Assumption here is that there will be only one logpart
 *    (primary and backup) that will have the
 *    maximum and others are trimmed by LCPs. If more than one
 *    logparts that will have the same usage, the test will fail.
 * - Find the logpart usage, node id and log part that went full.
 * - Restart the found node id. After completing the NR,
 *   check the usage of that nodeid/logpart.
 * - If the usage is not reduced within 2/3 of the LCP_INTERVAL seconds,
 *     (approximates to 20 seconds) the test will fail.
 */
static int runCheckLCPStartsAfterNR(NDBT_Context *ctx, NDBT_Step *step) {
  // Block redo logpart being trimmed by holding a transaction open
  HugoOperations *ops = NULL;
  start_open_transaction(ctx, step, &ops);

  // Perform 1 LCP
  ctx->setProperty("start_lcp", (Uint32)1);
  while (ctx->getProperty("lcps_done", (Uint32)0) != 1)
    NdbSleep_MilliSleep(1000);
  ctx->setProperty("lcps_done", (Uint32)0);

  // Perform some writes
  NdbError err;
  int upval = 0;
  run_write_ops(ctx, step, upval++, err, true);

  // Perform 1 LCP
  ctx->setProperty("start_lcp", (Uint32)1);
  while (ctx->getProperty("lcps_done", (Uint32)0) != 1)
    NdbSleep_MilliSleep(1000);

  // When redolog starts to get full (err code 410),
  // fill more (100 run_write_ops = 100k pkWrite ops) to force
  // the logpart to get filled to its max

  // Find the redo logpart usage and node id of the logpart that went full
  int retries = -1;

  while (!ctx->isTestStopped()) {
    run_write_ops(ctx, step, upval++, err, true);

    if (err.code == 410 && retries == -1) {
      retries = 100;
    }
    if (retries > 0 && retries-- == 1) break;

    // Continue load until lcps are finished
  }

  if (ctx->isTestStopped()) return NDBT_FAILED;

  // Find the redo logpart usage and node id of the logpart that went full
  int usage_before = -1;
  Uint32 nodeid = 0;  // The node with full redo logpart
  Uint32 full_logpart = UINT32_MAX;
  usage_before = get_redo_logpart_maxusage(ctx, nodeid, full_logpart);
  CHK3(full_logpart != UINT32_MAX, "No logpart became full");
  CHK3(nodeid != 0, "No nodeid found with almost full logpart");
  CHK3(usage_before > 0, "Redo log usage <= 0");

  // The node with full redo logpart. Same as nodeid but of type 'int'.
  int victim = (int)nodeid;

  g_info << "Stopping node " << victim << endl;
  NdbRestarter restarter;
  CHK3(restarter.restartOneDbNode(victim,
                                  /** initial */ false,
                                  /** nostart */ true,
                                  /** abort   */ true) == 0,
       "Restart a node failed");
  CHK3(restarter.waitNodesNoStart(&victim, 1) == 0,
       "Started node has not reached NoStart state");

  // World is moving on with more load and lcps while the victim is away
  bool lcp_started = false;
  while (ctx->getProperty("lcps_done", (Uint32)0) != 1 &&
         !ctx->isTestStopped()) {
    NdbError err;
    run_write_ops(ctx, step, upval++, err);

    if (!lcp_started) {
      lcp_started = true;
      ctx->setProperty("start_lcp", (Uint32)4);
      g_info << "Starting lcp" << endl;
    }
    // Continue load until lcps are finished
  }

  if (ctx->isTestStopped()) return NDBT_FAILED;

  ctx->setProperty("stop_lcp", (Uint32)1);  // stop runLCP()

  g_err << "Restarting the stopped node " << victim << endl;
  CHK3(restarter.startNodes(&victim, 1) == 0, "Start node failed");
  CHK3(restarter.waitNodesStarted(&victim, 1) == 0, "Node not started");

  // Check whether the full redo log part has been trimmed
  CHK3((redologpart_is_trimmed(ctx, usage_before, full_logpart, nodeid)) ==
           NDBT_OK,
       "Check for redolog trimmed failed");
  return NDBT_OK;
}

/** Test if a delay in opening a redo file is handled gracefully.
 * Fill the redo log. Delay opening a redo log file in order to
 * simulate a tardy disk, wth of error insertion (5090).
 * Empty the redo log, execute some more transactions.
 *
 * The test fills some redo log part up to almost full (error 410).
 * The error insertion is on the first redo log with fileNo>3 and the default
 * NoOfFragmentLogFiles=16. So the test assumes with confidence that
 * the error insertion must have occurred and the victim redo log file
 * is opend after the delay, before the redo log part becomes full.
 *
 * The test will fail if transactions get errors other than the following:
 * 410 - REDO log files overloaded, 266 - Time-out in NDB,
 * 1220 - REDO log files overloaded (increase FragmentLogFileSize).
 * The latter will be the direct consequence of the error injection
 * when the test is run with DefaultOperationRedoProblemAction=abort.
 * However the test is run with default value "queue".
 * Test will pass after the redo log is trimmed and some more transactions pass.
 */
static int runCheckOpenNextRedoLogFile(NDBT_Context *ctx, NDBT_Step *step) {
  // Block redo logpart being trimmed by holding a transaction open
  HugoOperations *ops = NULL;
  start_open_transaction(ctx, step, &ops);

  NdbRestarter restarter;
  int node = restarter.getNode(NdbRestarter::NS_RANDOM);
  g_err << "Inserting error in node " << node << endl;
  CHK3(restarter.insertErrorInNode(node, 5090) == 0, "Error insertion failed");

  // Run transactions until some redo log part gets full.
  // Commit the open transaction to trim the redo log.
  int retries = -1;
  int success_after_err = 0;
  bool committed = false;
  NdbError err;
  int upval = 0;
  g_err << "Filling redo logs" << endl;

  while (!ctx->isTestStopped()) {
    run_write_ops(ctx, step, upval++, err, true);

    if (err.code == 410) {
      if (retries == -1) retries = 100;

      // Find the logpart that became almost full
      int usage_before = -1;
      Uint32 full_logpart = UINT32_MAX;
      Uint32 nodeid = 0;

      usage_before = get_redo_logpart_maxusage(ctx, nodeid, full_logpart);
      CHK3(usage_before > 0, "Redo log usage <= 0");
      CHK3(nodeid != 0, "No nodeid found with almost full logpart");
      CHK3(full_logpart != UINT32_MAX, "No logpart became full");

      if (!committed) {
        // Commit the open transaction to trim the redo log part.
        CHK3(ops->execute_Commit(GETNDB(step)) == 0,
             "Error: failed to commit the open transaction.");
      }
      committed = true;
      g_err << "Check whether the redo log is trimmed" << endl;
      CHK3(((redologpart_is_trimmed(ctx, usage_before, full_logpart, nodeid)) ==
            NDBT_OK),
           "Check for redolog trimmed failed");

      // Start counting the succeeded transactions after the log part trim
      success_after_err = 0;

    } else if (err.code == 266 || err.code == 1220) {
      NdbSleep_MilliSleep(100);
      // Continue with new transactions
    } else if (err.code > 0) {
      g_err << "Transaction aborted with err " << err.code << " " << err.message
            << endl;
      break;
    } else {
      // err.code = 0 (no errors)
      if (retries > 0) {
        if (success_after_err++ > 50) {
          // Some more transactions are executed to confirm that
          // the inserted error scenario is alleviated.
          return NDBT_OK;
        }
        if (retries-- == 1) {
          g_err << "Transactions completed after redo log is trimmed are : "
                << success_after_err << ", Intended to complete > 50" << endl;
          break;
        }
      }
    }
  }
  return NDBT_FAILED;
}

static int runShowWrites(NDBT_Context *ctx, NDBT_Step *step) {
  while (!ctx->isTestStopped()) {
    NdbSleep_SecSleep(1);
    const Uint32 round_count = ctx->getProperty("WRITE_ROUNDS", (Uint32)0);
    ndbout_c("Write rounds %u", round_count);
  }
  return NDBT_OK;
}

static int runTempRedoError(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * Assuming that there is some background load writing
   * to the cluster, this test will :
   * 1) Wait a short time
   * 2) Use ERROR INSERT 5083 to stall redo logging
   * 3) Wait a short time
   * 4) Remove ERROR INSERT 5083
   * 5) Verify that writes to the cluster resume in
   *    a reasonable time
   * This gives some coverage of issues related to redo
   * problems not being automatically cleared
   */
  NdbRestarter restarter;
  int result = NDBT_FAILED;

  ndbout_c("RunTempRedoError");
  ndbout_c("Give some time for writes to get underway");
  const int DELAY_SECONDS = 10;
  NdbSleep_SecSleep(DELAY_SECONDS);

  ndbout_c("Triggering redo issue");
  CHK3(restarter.insertErrorInAllNodes(5083) == 0, "Error insertion 1 failed");
  ndbout_c("Waiting for writes to stall");
  NdbSleep_SecSleep(DELAY_SECONDS);

  const Uint32 stalled_round_count =
      ctx->getProperty("WRITE_ROUNDS", (Uint32)0);
  ndbout_c("Stalled write round count %u", stalled_round_count);
  ndbout_c("Removing redo issue");
  CHK3(restarter.insertErrorInAllNodes(0) == 0, "Error insertion 2 failed");

  /**
   * Write rounds should resume increasing within a reasonable time
   * otherwise we're stuck in the stalled state
   */
  ndbout_c("Waiting for write rounds to resume");
  Uint32 round_count = 0;
  Uint32 maxTimeToResumeSeconds = 60;
  do {
    NdbSleep_SecSleep(1);
    round_count = ctx->getProperty("WRITE_ROUNDS", (Uint32)0);
    if (round_count > stalled_round_count) {
      ndbout_c("Write rounds increased within time limit : Success");
      result = NDBT_OK;
      break;
    }
  } while (--maxTimeToResumeSeconds > 0);

  ctx->stopTest();
  return result;
}

NDBT_TESTSUITE(testRedo);
TESTCASE("WriteOK", "Run only write to verify REDO size is adequate") {
  TC_PROPERTY("TABMASK", (Uint32)(2));
  INITIALIZER(runCreate);
  STEP(runWriteOK);
  FINALIZER(runDrop);
}
TESTCASE("Bug36500", "Long trans and recovery from 410") {
  TC_PROPERTY("TABMASK", (Uint32)(1 | 2));
  INITIALIZER(runCreate);
  STEP(runLongtrans);
  STEP(runWrite410);
  FINALIZER(runDrop);
}
TESTCASE("Latency410", "Transaction latency under 410") {
  TC_PROPERTY("TABMASK", (Uint32)(1 | 2 | 4));
  TC_PROPERTY("SLEEP410", (Uint32)60);
  INITIALIZER(runCreate);
  STEP(runLongtrans);
  STEP(runWrite410);
  STEP(runLatency);
  FINALIZER(runDrop);
}
TESTCASE("RestartOK", "Node restart") {
  TC_PROPERTY("TABMASK", (Uint32)(2));
  INITIALIZER(runCreate);
  STEP(runWriteOK);
  STEP(runRestartOK);
  FINALIZER(runDrop);
}
TESTCASE("RestartFD", "Long trans and node restart with few LQH FDs") {
  TC_PROPERTY("TABMASK", (Uint32)(1 | 2));
  STEP(runRestartFD);
  FINALIZER(runDrop);
  FINALIZER(runResetFD);
}
TESTCASE("RestartFDSR", "RestartFD using system restart") {
  TC_PROPERTY("TABMASK", (Uint32)(1 | 2));
  TC_PROPERTY("SRFLAG", (Uint32)1);
  STEP(runRestartFD);
  FINALIZER(runDrop);
  FINALIZER(runResetFD);
}
TESTCASE("RedoFull", "Fill redo logs, apply load and check queuing aborted") {
  TC_PROPERTY("TABMASK", (Uint32)(3));
  TC_PROPERTY("REDOLOGCOUNT", (Uint32)(3));
  TC_PROPERTY("REDOLOGSIZE", (Uint32)(4 * 1024 * 1024));
  INITIALIZER(resizeRedoLog);
  INITIALIZER(runCreate);
  STEP(runWriteWithRedoFull);
  FINALIZER(runDrop);
  FINALIZER(resizeRedoLog);
}
TESTCASE("CheckLCPStartsAfterSR",
         "Fill redo logs to full, SR, and see if LCP starts") {
  TC_PROPERTY("TABMASK", (Uint32)(3));
  TC_PROPERTY("LCPINTERVAL", (Uint32)(31));
  INITIALIZER(resizeRedoLog);
  INITIALIZER(runCreate);
  STEP(runCheckLCPStartsAfterSR);
  STEP(runLCP);
  FINALIZER(runDrop);
  FINALIZER(resizeRedoLog);
}
TESTCASE("CheckLCPStartsAfterNR",
         "Fill redo logs to full, restart the node having full redo,"
         "and see if LCP starts") {
  TC_PROPERTY("TABMASK", (Uint32)(3));
  TC_PROPERTY("LCPINTERVAL", (Uint32)(31));
  TC_PROPERTY("NR", (Uint32)(1));
  INITIALIZER(resizeRedoLog);
  INITIALIZER(runCreate);
  STEP(runCheckLCPStartsAfterNR);
  STEP(runLCP);
  FINALIZER(runDrop);
  FINALIZER(resizeRedoLog);
}
TESTCASE("CheckNextRedoFileOpened",
         "Fill redo logs to full, check if next file is open"
         "in a stressed disk situation") {
  TC_PROPERTY("TABMASK", (Uint32)(3));
  INITIALIZER(resizeRedoLog);
  INITIALIZER(runCreate);
  STEP(runCheckOpenNextRedoLogFile);
  FINALIZER(runDrop);
  FINALIZER(resizeRedoLog);
}
TESTCASE("RedoStallRecover",
         "Simulate redo problem, resulting in transaction "
         "timeouts, then check the problem clears") {
  TC_PROPERTY("TABMASK", (Uint32)(3));
  TC_PROPERTY("WRITE_COUNT_ROUNDS", (Uint32)1);
  TC_PROPERTY("WRITE_ROUNDS", (Uint32)0);
  TC_PROPERTY("RANGE_PER_STEP", (Uint32)1);
  INITIALIZER(runCreate);
  STEPS(runWriteOK, 8);
  STEP(runShowWrites);
  STEP(runTempRedoError);
  FINALIZER(runDrop);
}

NDBT_TESTSUITE_END(testRedo)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testRedo);
  testRedo.setCreateTable(false);
  myRandom48Init((long)NdbTick_CurrentMillisecond());
  g_msgmutex = NdbMutex_Create();
  require(g_msgmutex != 0);
  int ret = testRedo.execute(argc, argv);
  NdbMutex_Destroy(g_msgmutex);
  return ret;
}
