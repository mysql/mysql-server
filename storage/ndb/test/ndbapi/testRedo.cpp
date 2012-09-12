/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoOperations.hpp>
#include <NdbRestarter.hpp>
#include <mgmapi.h>
#include <ndb_logevent.h>
#include <NdbTick.h>
#include <NDBT_Stats.hpp>
#include <random.h>

static NdbMutex* g_msgmutex = 0;

#undef require
#define require(b) \
  if (!(b)) { \
    NdbMutex_Lock(g_msgmutex); \
    g_err << "ABORT: " << #b << " failed at line " << __LINE__ << endl; \
    NdbMutex_Unlock(g_msgmutex); \
    abort(); \
  }

#define CHK1(b) \
  if (!(b)) { \
    NdbMutex_Lock(g_msgmutex); \
    g_err << "ERROR: " << #b << " failed at line " << __LINE__ << endl; \
    NdbMutex_Unlock(g_msgmutex); \
    result = NDBT_FAILED; \
    break; \
  }

#define CHK2(b, e) \
  if (!(b)) { \
    NdbMutex_Lock(g_msgmutex); \
    g_err << "ERROR: " << #b << " failed at line " << __LINE__ \
          << ": " << e << endl; \
    NdbMutex_Unlock(g_msgmutex); \
    result = NDBT_FAILED; \
    break; \
  }

#define info(x) \
  do { \
    NdbMutex_Lock(g_msgmutex); \
    g_info << x << endl; \
    NdbMutex_Unlock(g_msgmutex); \
  } while (0)

static const int g_tabmax = 3;
static const char* g_tabname[g_tabmax] = { "T1", "T2", "T4" };
static const NdbDictionary::Table* g_tabptr[g_tabmax] = { 0, 0, 0 };

static int
runCreate(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  int tabmask = ctx->getProperty("TABMASK", (Uint32)0);

  for (int i = 0; i < g_tabmax; i++)
  {
    if (!(tabmask & (1 << i)))
      continue;
    const char* tabname = g_tabname[i];
    (void)pDic->dropTable(tabname);

    const NdbDictionary::Table* pTab = NDBT_Tables::getTable(tabname);
    require(pTab != 0);
    NdbDictionary::Table tab2(*pTab);
    // make sure to hit all log parts
    tab2.setFragmentType(NdbDictionary::Object::FragAllLarge);
    //tab2.setMaxRows(100000000);
    CHK2(pDic->createTable(tab2) == 0, pDic->getNdbError());

    g_tabptr[i] = pDic->getTable(tabname);
    require(g_tabptr[i] != 0);
    info("created " << tabname);
  }

  return result;
}

static int
runDrop(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  int result = NDBT_OK;
  int tabmask = ctx->getProperty("TABMASK", (Uint32)0);

  for (int i = 0; i < g_tabmax; i++)
  {
    if (!(tabmask & (1 << i)))
      continue;
    const char* tabname = g_tabname[i];
    if (g_tabptr[i] != 0)
    {
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

static int
get_err410(NDBT_Context* ctx)
{
  int v = (int)ctx->getProperty("ERR410", (Uint32)0);
  require(v == 0 || v == 1 || v == 2);
  return v;
}

static void
set_err410(NDBT_Context* ctx, int v)
{
  require(v == 0 || v == 1 || v == 2);
  require(get_err410(ctx) != v);
  ctx->setProperty("ERR410", (Uint32)v);
}

static int
runLongtrans(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  const int sleep410 = ctx->getProperty("SLEEP410", (Uint32)0);

  const NdbDictionary::Table* pTab = g_tabptr[0];
  require(pTab != 0);

  info("longtrans: start");
  int loop = 0;
  while (!ctx->isTestStopped())
  {
    info("longtrans: loop " << loop);
    HugoOperations ops(*pTab);
    ops.setQuiet();
    CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());
    CHK2(ops.pkInsertRecord(pNdb, 0, 1, 0) == 0, ops.getNdbError());

    while (!ctx->isTestStopped())
    {
      int v = get_err410(ctx);
      require(v == 0 || v == 1);
      if (v != 0)
      {
        info("longtrans: 410 seen");
        if (sleep410 > 0)
        {
          info("longtrans: sleep " << sleep410);
          sleep(sleep410);
        }

        CHK2(ops.execute_Rollback(pNdb) == 0, ops.getNdbError());
        ops.closeTransaction(pNdb);
        info("longtrans: rollback done");
        set_err410(ctx, 2);

        while (!ctx->isTestStopped())
        {
          int v = get_err410(ctx);
          if (v != 0)
          {
          }
          else
          {
            info("longtrans: 410 cleared");
            break;
          }
          sleep(1);
        }
        break;
      }
      sleep(1);
    }
    CHK1(result == NDBT_OK);

    if (ops.getTransaction() != NULL)
    {
      info("longtrans: close leftover transaction");
      ops.closeTransaction(pNdb);
    }
    
    loop++;
  }

  info("longtrans: stop");
  return result;
}

static int
run_write_ops(NDBT_Context* ctx, NDBT_Step* step, int upval, NdbError& err)
{
  Ndb* pNdb = GETNDB(step);
  const int records = ctx->getNumRecords();
  int result = NDBT_OK;

  const NdbDictionary::Table* pTab = g_tabptr[1];
  require(pTab != 0);

  while (!ctx->isTestStopped())
  {
    HugoOperations ops(*pTab);
    ops.setQuiet();
    CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());

    for (int record = 0; record < records; record++)
    {
      CHK2(ops.pkWriteRecord(pNdb, record, 1, upval) == 0, ops.getNdbError());
    }
    CHK1(result == NDBT_OK);

    int ret = ops.execute_Commit(pNdb);
    err = ops.getNdbError();
    ops.closeTransaction(pNdb);

    if (ret == 0)
      break;

    require(err.code != 0);
    CHK2(err.status == NdbError::TemporaryError, err);

    if (err.code == 410)
      break;

    info("write: continue on " << err);
    NdbSleep_MilliSleep(100);
  }

  return result;
}

static int
runWriteOK(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;

  info("write: start");
  int loop = 0;
  int upval = 0;

  while (!ctx->isTestStopped())
  {
    if (loop % 100 == 0)
      info("write: loop " << loop);

    NdbError err;
    CHK2(run_write_ops(ctx, step, upval++, err) == 0, err);
    require(err.code == 0 || err.code == 410);
    CHK2(err.code == 0, err);
    NdbSleep_MilliSleep(100);

    loop++;
  }

  return result;
}

static int
runWrite410(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  const int loops = ctx->getNumLoops();

  info("write: start");
  int loop = 0;
  int upval = 0;

  while (loop < loops && !ctx->isTestStopped())
  {
    info("write: loop " << loop);

    while (!ctx->isTestStopped())
    {
      NdbError err;
      CHK2(run_write_ops(ctx, step, upval++, err) == 0, err);
      if (err.code != 0)
      {
        require(err.code == 410);
        info("write: setting 410");
        set_err410(ctx, 1);
        break;
      }
      NdbSleep_MilliSleep(100);
    }

    while (1)
    {
      int v = get_err410(ctx);
      if (v != 2)
      {
        require(v == 1);
      }
      else
      {
        info("write: longtrans rollback seen");
        break;
      }
      sleep(1);
    }

    while (!ctx->isTestStopped())
    {
      NdbError err;
      CHK2(run_write_ops(ctx, step, upval++, err) == 0, err);
      if (err.code == 0)
      {
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
  OpLat(int op, int repeat) :
    m_op(op),
    m_repeat(repeat) {}
};

static void
run_latency_report(const OpLat* oplist, int opcnt)
{
  for (int i = 0; i < opcnt; i++)
  {
    const OpLat& oplat = oplist[i];
    printf("optype: %d\n", oplat.m_op);
    for (int i0 = 0; i0 < 2; i0++)
    {
      printf("410 off/on: %d\n", i0);
      printf("op status ok / 410 / other temp error:\n");
      for (int i1 = 0; i1 < 3; i1++)
      {
        const NDBT_Stats& lat = oplat.m_lat[i0][i1];
        printf("count: %d", lat.getCount());
        if (lat.getCount() > 0)
        {
          printf(" mean: %.2f min: %.2f max: %.2f stddev: %.2f",
                 lat.getMean(), lat.getMin(), lat.getMax(), lat.getStddev());
        }
        printf("\n");
      }
    }
  }
}

static int
run_latency_ops(NDBT_Context* ctx, NDBT_Step* step, OpLat& oplat, int upval, NdbError& err)
{
  Ndb* pNdb = GETNDB(step);
  const int records = ctx->getNumRecords();
  int result = NDBT_OK;

  const NdbDictionary::Table* pTab = g_tabptr[2];
  require(pTab != 0);

  int record = 0;
  while (record < records && !ctx->isTestStopped())
  {
    HugoOperations ops(*pTab);
    ops.setQuiet();

    MicroSecondTimer timer_start;
    MicroSecondTimer timer_stop;
    require(NdbTick_getMicroTimer(&timer_start) == 0);

    CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());

    switch (oplat.m_op)
    {
      case NdbOperation::InsertRequest:
        CHK2(ops.pkInsertRecord(pNdb, record, 1, upval) == 0, ops.getNdbError());
        break;
      case NdbOperation::UpdateRequest:
        CHK2(ops.pkUpdateRecord(pNdb, record, 1, upval) == 0, ops.getNdbError());
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
    CHK2(result == NDBT_OK, "latency: ndbapi error at op " << oplat.m_op << " record" << record);

    int ret = ops.execute_Commit(pNdb);
    err = ops.getNdbError();
    ops.closeTransaction(pNdb);

    if (ret != 0)
    {
      require(err.code != 0);
      CHK2(err.status == NdbError::TemporaryError, err);
    }

    require(NdbTick_getMicroTimer(&timer_stop) == 0);
    NDB_TICKS tt = NdbTick_getMicrosPassed(timer_start, timer_stop);
    require(tt > 0);
    double td = (double)tt;
    int i0 = get_err410(ctx);
    int i1 = (ret == 0 ? 0 : (err.code == 410 ? 1 : 2));
    NDBT_Stats& lat = oplat.m_lat[i0][i1];
    lat.addObservation(td);

    if (ret == 0)
      record++;
  }

  return result;
}

static int
runLatency(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;

  info("latency: start");
  const int opcnt = 4; 
  OpLat oplist[opcnt] = {
    OpLat(NdbOperation::InsertRequest, 1),
    OpLat(NdbOperation::UpdateRequest, 10),
    OpLat(NdbOperation::ReadRequest, 5),
    OpLat(NdbOperation::DeleteRequest, 1)
  };

  int loop = 0;
  int upval = 0;

  while (!ctx->isTestStopped())
  {
    info("latency: loop " << loop);
    for (int i = 0; i < opcnt && !ctx->isTestStopped(); i++)
    {
      OpLat& oplat = oplist[i];
      NdbError err;
      for (int j = 0; j < oplat.m_repeat && !ctx->isTestStopped(); j++)
      {
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
  int m_pos; // absolute mb
  friend NdbOut& operator<<(NdbOut&, const LogPos&);
};

NdbOut&
operator<<(NdbOut& out, const LogPos& pos)
{
  out << pos.m_fileno;
  out << "." << pos.m_mb;
  out << "-" << pos.m_pos;
  return out;
}

struct LogPart {
  int m_partno; // for print
  bool m_set;
  int m_files; // redo files
  int m_filesize; // mb
  int m_total; // m_files * m_filesize
  int m_free; // mb
  int m_used; // mb
  LogPos m_head;
  LogPos m_tail;
  int m_fileused;
  void reset() {
    m_set = false;
  }
  bool isset() {
    return m_set;
  }
  friend NdbOut& operator<<(NdbOut&, const LogPart&);
};

NdbOut&
operator<<(NdbOut& out, const LogPart& lp)
{
  out << "part " << lp.m_partno << ":";
  out << " files=" << lp.m_files;
  out << " filesize=" << lp.m_filesize;
  out << " total=" << lp.m_total;
  out << " free=" << lp.m_free;
  out << " head: " << lp.m_head;
  out << " tail: "  << lp.m_tail;
  out << " fileused=" << lp.m_fileused;
  return out;
}

struct LogNode {
  int m_nodeid;
  LogPart m_logpart[4];
  int m_files; // from LogPart (must be same for all)
  int m_filesize;
  int m_minfds; // min and max FDs in page 0
  int m_maxfds; // LQH uses max FDs by default
  void reset() {
    for (int i = 0; i < 4; i++) {
      m_logpart[i].m_partno = i;
      m_logpart[i].reset();
    }
  }
  bool isset() {
    for (int i = 0; i < 4; i++)
      if (!m_logpart[i].isset())
        return false;
    return true;
  }
};

struct LogInfo {
  int m_nodes;
  LogNode* m_lognode;
  int m_files; // from LogNode (config is same for all in these tests)
  int m_filesize;
  int m_minfds;
  int m_maxfds;
  LogInfo(int nodes) {
    m_nodes = nodes;
    m_lognode = new LogNode [nodes];
    reset();
  }
  ~LogInfo() {
    m_nodes = 0;
    delete [] m_lognode;
  }
  void reset() {
    for (int n = 0; n < m_nodes; n++)
      m_lognode[n].reset();
  }
  bool isset() {
    for (int n = 0; n < m_nodes; n++)
      if (!m_lognode[n].isset())
        return false;
    return true;
  }
  LogNode* findnode(int nodeid) const {
    for (int n = 0; n < m_nodes; n++) {
      if (m_lognode[n].m_nodeid == nodeid)
        return &m_lognode[n];
    }
    return 0;
  }
  void copyto(LogInfo& li2) const {
    require(m_nodes == li2.m_nodes);
    for (int n = 0; n < m_nodes; n++) {
      const LogNode& ln1 = m_lognode[n];
      LogNode& ln2 = li2.m_lognode[n];
      ln2 = ln1;
    }
  }
};

static int
get_nodestatus(NdbMgmHandle h, LogInfo& li)
{
  int result = 0;
  ndb_mgm_cluster_state* cs = 0;

  do
  {
    require(h != 0);
    CHK2((cs = ndb_mgm_get_status(h)) != 0, ndb_mgm_get_latest_error_msg(h));
    int n = 0;
    for (int i = 0; i < cs->no_of_nodes; i++)
    {
      ndb_mgm_node_state& ns = cs->node_states[i];
      if (ns.node_type == NDB_MGM_NODE_TYPE_NDB)
      {
        // called only when all started
        CHK1(ns.node_status == NDB_MGM_NODE_STATUS_STARTED);
        CHK1(n < li.m_nodes);

        LogNode& ln = li.m_lognode[n];
        ln.m_nodeid = ns.node_id;
        info("node " << n << ": " << ln.m_nodeid);
        n++;
      }
      CHK1(result == 0);
    }
    CHK1(n == li.m_nodes);
  }
  while (0);

  free(cs);
  info("get_nodestatus result=" << result);
  return result;
}

static int
get_redostatus(NdbMgmHandle h, LogInfo& li)
{
  int result = 0;

  do
  {
    li.reset();
    require(h != 0);

    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0 };
    NdbLogEventHandle evh = 0;
    CHK2((evh = ndb_mgm_create_logevent_handle(h, filter)) != 0, ndb_mgm_get_latest_error_msg(h));

    for (int n = 0; n < li.m_nodes; n++)
    {
      int dump[] = { 2399 };
      const LogNode& ln = li.m_lognode[n];
      struct ndb_mgm_reply reply;
      CHK2(ndb_mgm_dump_state(h, ln.m_nodeid, dump, 1, &reply) == 0, ndb_mgm_get_latest_error_msg(h));
    }
    CHK1(result == 0);

    int maxcnt = 4 * li.m_nodes;
    int rescnt = 0;
    time_t start = time(0);
    int maxwait = 5;

    while (rescnt < maxcnt && time(0) < start + maxwait)
    {
      while (1)
      {
        int res;
        ndb_logevent ev;
        int msec = 100;
        CHK2((res = ndb_logevent_get_next(evh, &ev, msec)) >= 0, ndb_mgm_get_latest_error_msg(h));
        if (res == 0)
        break;
      if (ev.type != NDB_LE_RedoStatus)
          continue;

        LogNode* lnptr = 0;
        CHK2((lnptr = li.findnode(ev.source_nodeid)) != 0, "unknown nodeid " << ev.source_nodeid);
        LogNode& ln = *lnptr;

        const ndb_logevent_RedoStatus& rs = ev.RedoStatus;
        CHK1(rs.log_part < 4);
        LogPart& lp = ln.m_logpart[rs.log_part];

        CHK1(!lp.m_set);
        LogPos& head = lp.m_head;
        LogPos& tail = lp.m_tail;
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
        if (tail.m_fileno < head.m_fileno)
        {
          lp.m_fileused = head.m_fileno - tail.m_fileno + 1;
        }
        else if (tail.m_fileno > head.m_fileno)
        {
          lp.m_fileused = lp.m_files - (tail.m_fileno - head.m_fileno - 1);
        }
        else if (tail.m_pos < head.m_pos)
        {
          lp.m_fileused = 1;
        }
        else if (tail.m_pos > head.m_pos)
        {
          lp.m_fileused = lp.m_files;
        }
        else
        {
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
          if (tail.m_pos <= head.m_pos)
          {
            CHK2(lp.m_free == lp.m_total - (head.m_pos - tail.m_pos), lp);
          }
          else
          {
            CHK2(lp.m_free == tail.m_pos - head.m_pos, lp);
          }
        }
        lp.m_set = true;
        //info("node " << ln.m_nodeid << ": " << lp);

        rescnt++;
      }
      CHK1(result == 0);
    }
    CHK1(result == 0);
    CHK2(rescnt == maxcnt, "got events " << rescnt << " != " << maxcnt);
    require(li.isset()); // already implied by counts

    for (int n = 0; n < li.m_nodes; n++)
    {
      LogNode& ln = li.m_lognode[n];
      for (int i = 0; i < 4; i++)
      {
        LogPart& lp = ln.m_logpart[i];
        if (i == 0)
        {
          ln.m_files = lp.m_files;
          ln.m_filesize = lp.m_filesize;
          CHK1(ln.m_files >= 3 && ln.m_filesize >= 4);

          // see Dblqh::execREAD_CONFIG_REQ()
          ln.m_minfds = 2;
          ln.m_maxfds = (8192 - 32 - 128) / (3 * ln.m_filesize);
          if (ln.m_maxfds > 40)
            ln.m_maxfds = 40;
          CHK1(ln.m_minfds <= ln.m_maxfds);
        }
        else
        {
          CHK1(ln.m_files == lp.m_files && ln.m_filesize == lp.m_filesize);
        }
      }

      if (n == 0)
      {
        li.m_files = ln.m_files;
        li.m_filesize = ln.m_filesize;
        li.m_minfds = ln.m_minfds;
        li.m_maxfds = ln.m_maxfds;
        require(li.m_files > 0 && li.m_filesize > 0);
        require(li.m_minfds <= li.m_maxfds);
      }
      else
      {
        CHK1(li.m_files == ln.m_files && li.m_filesize == ln.m_filesize);
        require(li.m_minfds == ln.m_minfds && li.m_maxfds == ln.m_maxfds);
      }

      CHK1(result == 0);
    }
    CHK1(result == 0);

    ndb_mgm_destroy_logevent_handle(&evh);
  }
  while (0);

  info("get_redostatus result=" << result);
  return result;
}

// get node with max redo files used in some part

struct LogUsed {
  int m_nodeidx;
  int m_nodeid;
  int m_partno;
  int m_used; // mb
  LogPos m_head;
  LogPos m_tail;
  int m_fileused;
  int m_rand; // randomize node to restart if file usage is same
  friend NdbOut& operator<<(NdbOut&, const LogUsed&);
};

NdbOut&
operator<<(NdbOut& out, const LogUsed& lu)
{
  out << "n=" << lu.m_nodeid;
  out << " p=" << lu.m_partno;
  out << " u=" << lu.m_used;
  out << " h=" << lu.m_head;
  out << " t=" << lu.m_tail;
  out << " f=" << lu.m_fileused;
  return out;
}

static int
cmp_logused(const void* a1, const void* a2)
{
  const LogUsed& lu1 = *(const LogUsed*)a1;
  const LogUsed& lu2 = *(const LogUsed*)a2;
  int k = lu1.m_fileused - lu2.m_fileused;
  if (k != 0)
  {
    // sorting by larger file usage
    return (-1) * k;
  }
  return lu1.m_rand - lu2.m_rand;
}

struct LogMax {
  int m_nodes;
  LogUsed* m_logused;
  LogMax(int nodes) {
    m_nodes = nodes;
    m_logused = new LogUsed [nodes];
  };
  ~LogMax() {
    m_nodes = 0;
    delete [] m_logused;
  }
};

static void
get_redoused(const LogInfo& li, LogMax& lx)
{
  require(li.m_nodes == lx.m_nodes);
  for (int n = 0; n < li.m_nodes; n++)
  {
    const LogNode& ln = li.m_lognode[n];
    LogUsed& lu = lx.m_logused[n];
    lu.m_used = -1;
    for (int i = 0; i < 4; i++)
    {
      const LogPart& lp = ln.m_logpart[i];
      if (lu.m_used <  lp.m_used)
      {
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
  for (int n = 0; n + 1 < li.m_nodes; n++)
  {
    const LogUsed& lu1 = lx.m_logused[n];
    const LogUsed& lu2 = lx.m_logused[n + 1];
    require(lu1.m_fileused >= lu2.m_fileused);
  }
}

struct LogDiff {
  bool m_tailmove; // all tails since all redo parts are used
};

static void
get_redodiff(const LogInfo& li1, const LogInfo& li2, LogDiff& ld)
{
  require(li1.m_nodes == li2.m_nodes);
  ld.m_tailmove = true;
  for (int i = 0; i < li1.m_nodes; i++)
  {
    LogNode& ln1 = li1.m_lognode[i];
    LogNode& ln2 = li2.m_lognode[i];
    for (int j = 0; j < 4; j++)
    {
      LogPart& lp1 = ln1.m_logpart[j];
      LogPart& lp2 = ln2.m_logpart[j];
      if (lp1.m_tail.m_pos == lp2.m_tail.m_pos)
      {
        ld.m_tailmove = false;
      }
    }
  }
}

static int
runRestartOK(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  const int loops = ctx->getNumLoops();
  NdbRestarter restarter;

  info("restart01: start");
  int nodes = restarter.getNumDbNodes();
  require(nodes >= 1);
  info("restart: nodes " << nodes);

  if (nodes == 1)
  {
    info("restart01: need at least 2 nodes");
    return result;
  }

  int nodeidx = myRandom48(nodes);
  int nodeid = restarter.getDbNodeId(nodeidx);
  info("restart01: using nodeid " << nodeid);

  LogInfo logInfo(nodes);

  int loop = 0;
  while (loop < loops && !ctx->isTestStopped())
  {
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
    sleep(300);
    loop++;
  }

  info("restart01: stop test");
  ctx->stopTest();
  return result;
}

#define g_SETFDS "SETFDS"

static int
run_write_ops(NDBT_Context* ctx, NDBT_Step* step, int cnt, int& upval, NdbError& err)
{
  int result = NDBT_OK;

  for (int i = 0; i < cnt && !ctx->isTestStopped(); i++)
  {
    CHK2(run_write_ops(ctx, step, upval++, err) == 0, err);
    if (err.code != 0)
    {
      require(err.code == 410);
      break;
    }
  }

  return result;
}

static int
get_newfds(const LogInfo& li)
{
  require(li.m_files >= 3);
  int newfds = li.m_files - 1;
  require(newfds >= li.m_minfds);
  // twice to prefer smaller
  newfds = li.m_minfds + myRandom48(newfds - li.m_minfds + 1);
  newfds = li.m_minfds + myRandom48(newfds - li.m_minfds + 1);
  return newfds;
}

static int
get_limfds(const LogInfo& li, int newfds)
{
  int off = li.m_files - newfds;
  require(off > 0);
  off= myRandom48(off + 1);
  off = myRandom48(off + 1);
  return newfds + off;
}

static int
run_restart(NDBT_Context* ctx, NDBT_Step* step, int nodeid, bool fi)
{
  int result = NDBT_OK;
  int setfds = ctx->getProperty(g_SETFDS, (Uint32)0xff);
  require(setfds != 0xff);
  int dump[2] = { 2396, setfds };
  NdbRestarter restarter;
  info("run_restart: nodeid=" << nodeid << " initial=" << fi << " setfds=" << setfds);

  /*
   * When starting non-initial the node(s) have already some setfds
   * but it is lost on restart.  We must dump the same setfds again.
   */
  do
  {
    bool fn = true;
    bool fa = false;
    if (nodeid == 0)
    {
      info("run_restart: restart all nostart");
      CHK1(restarter.restartAll(fi, fn, fa) == 0);
      info("run_restart: wait nostart");
      CHK1(restarter.waitClusterNoStart() == 0);
      info("run_restart: dump " << dump[0] << " " << dump[1]);
      CHK1(restarter.dumpStateAllNodes(dump, 2) == 0);
      info("run_restart: start all");
      CHK1(restarter.startAll() == 0);
    }
    else
    {
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
  }
  while (0);

  info("run_restart: result=" << result);
  return result;
}

static int
run_start_lcp(NdbRestarter& restarter)
{
  int result = NDBT_OK;
  int dump[1] = { 7099 };
  do
  {
    CHK1(restarter.dumpStateAllNodes(dump, 1) == 0);
  }
  while (0);
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
 
static int
runRestartFD(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  const int loops = ctx->getNumLoops();
  const bool srflag = ctx->getProperty("SRFLAG", (Uint32)0);
  NdbRestarter restarter;

  info("restart: start srflag=" << srflag);
  int nodes = restarter.getNumDbNodes();
  require(nodes >= 1);
  info("restart: nodes " << nodes);

  if (nodes == 1 && !srflag)
  {
    info("restart: need at least 2 nodes");
    return result;
  }

  LogInfo logInfo(nodes);
  LogInfo logInfo2(nodes);
  LogMax logMax(nodes);
  LogDiff logDiff;

  const NdbDictionary::Table* pTab = 0;

  int upval = 0;
  int loop = 0;
  int newfds = 0;
  int limfds = 0;
  while (loop < loops && !ctx->isTestStopped())
  {
    info("restart: loop " << loop);
    if (loop % 5 == 0)
    {
      CHK1(get_nodestatus(restarter.handle, logInfo) == 0);
      CHK1(get_redostatus(restarter.handle, logInfo) == 0);

      // set new cmaxLogFilesInPageZero in all LQH nodes
      newfds = get_newfds(logInfo);
      ctx->setProperty(g_SETFDS, (Uint32)newfds);
      bool nodeid = 0; // all nodes
      bool fi = true; // initial start
      CHK1(run_restart(ctx, step, nodeid, fi) == 0);

      CHK1(runCreate(ctx, step) == 0);
      pTab = g_tabptr[0];
      require(pTab != 0);
    }

    // start long trans
    HugoOperations ops(*pTab);
    ops.setQuiet();
    CHK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());
    for (int i = 0; i < 100; i++)
    {
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
    while (!ctx->isTestStopped())
    {
      info("restart: load1 at " << upval);
      NdbError err;
      int cnt = 100 + myRandom48(100);
      CHK1(run_write_ops(ctx, step, cnt, upval, err) == 0);

      CHK1(get_redostatus(restarter.handle, logInfo) == 0);
      get_redoused(logInfo, logMax);
      info("restart: load1 max: " << logMax.m_logused[0]);
      info("restart: load1 min: " << logMax.m_logused[nodes - 1]);

      if (err.code != 0)
      {
        require(err.code == 410);
        info("restart: break load1 on 410");
        break;
      }

      int fileused = logMax.m_logused[0].m_fileused;
      if (fileused > limfds)
      {
        info("restart: break load1 on file usage > FDs");
        break;
      }
    }
    CHK1(result == NDBT_OK);

    // restart
    if (srflag)
    {
      int nodeid = 0;
      int fi = false;
      CHK1(run_restart(ctx, step, nodeid, fi) == 0);
    }
    else
    {
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
    while (!ctx->isTestStopped())
    {
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
      if (logDiff.m_tailmove)
      {
        info("restart: break load2");
        break;
      }

      info("restart: retry2=" << retry2);
      if (retry2 % 5 == 0)
      {
        CHK1(run_start_lcp(restarter) == 0);
        NdbSleep_MilliSleep(1000);
      }
      retry2++;
    }
    CHK1(result == NDBT_OK);

    sleep(1 + myRandom48(10));
    loop++;
  }

  info("restart: stop test");
  ctx->stopTest();
  return result;
}

static int
runResetFD(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int oldfds = ctx->getProperty(g_SETFDS, (Uint32)-1);
  do
  {
    if (oldfds == -1)
    {
      // never changed (some step failed)
      break;
    }
    ctx->setProperty(g_SETFDS, (Uint32)0);
    CHK1(run_restart(ctx, step, 0, true) == 0);
  }
  while (0);
  return result;
}

NDBT_TESTSUITE(testRedo);
TESTCASE("WriteOK", 
	 "Run only write to verify REDO size is adequate"){
  TC_PROPERTY("TABMASK", (Uint32)(2));
  INITIALIZER(runCreate);
  STEP(runWriteOK);
  FINALIZER(runDrop);
}
TESTCASE("Bug36500", 
	 "Long trans and recovery from 410"){
  TC_PROPERTY("TABMASK", (Uint32)(1|2));
  INITIALIZER(runCreate);
  STEP(runLongtrans);
  STEP(runWrite410);
  FINALIZER(runDrop);
}
TESTCASE("Latency410", 
	 "Transaction latency under 410"){
  TC_PROPERTY("TABMASK", (Uint32)(1|2|4));
  TC_PROPERTY("SLEEP410", (Uint32)60);
  INITIALIZER(runCreate);
  STEP(runLongtrans);
  STEP(runWrite410);
  STEP(runLatency);
  FINALIZER(runDrop);
}
TESTCASE("RestartOK", 
	 "Node restart"){
  TC_PROPERTY("TABMASK", (Uint32)(2));
  INITIALIZER(runCreate);
  STEP(runWriteOK);
  STEP(runRestartOK);
  FINALIZER(runDrop);
}
TESTCASE("RestartFD", 
	 "Long trans and node restart with few LQH FDs"){
  TC_PROPERTY("TABMASK", (Uint32)(1|2));
  STEP(runRestartFD);
  FINALIZER(runDrop);
  FINALIZER(runResetFD);
}
TESTCASE("RestartFDSR", 
	 "RestartFD using system restart"){
  TC_PROPERTY("TABMASK", (Uint32)(1|2));
  TC_PROPERTY("SRFLAG", (Uint32)1);
  STEP(runRestartFD);
  FINALIZER(runDrop);
  FINALIZER(runResetFD);
}
NDBT_TESTSUITE_END(testRedo);


int
main(int argc, const char** argv)
{
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
