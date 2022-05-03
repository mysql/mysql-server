/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <ndb_global.h>
#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <NdbThread.h>
#include <NdbTest.hpp>

struct Opt {
  bool m_dbg;
  const char* m_scan;
  const char* m_tname;
  const char* m_xname;
  Opt() :
    m_dbg(true),
    m_scan("tx"),
    m_tname("T"),
    m_xname("X")
    {}
};

static void
printusage()
{
  Opt d;
  ndbout
    << "usage: testDeadlock" << endl
    << "-scan tx        scan table, index [" << d.m_scan << "]" << endl
    ;
}

static Opt g_opt;

static NdbMutex *ndbout_mutex= NULL;
static Ndb_cluster_connection *g_cluster_connection= 0;
#define DBG(x) \
  do { \
    if (! g_opt.m_dbg) break; \
    NdbMutex_Lock(ndbout_mutex); \
    ndbout << "line " << __LINE__ << " " << x << endl; \
    NdbMutex_Unlock(ndbout_mutex); \
  } while (0)

#define CHK(x) \
  do { \
    if (x) break; \
    ndbout << "line " << __LINE__ << ": " << #x << " failed" << endl; \
    return -1; \
  } while (0)

#define CHN(p, x) \
  do { \
    if (x) break; \
    ndbout << "line " << __LINE__ << ": " << #x << " failed" << endl; \
    ndbout << (p)->getNdbError() << endl; \
    return -1; \
  } while (0)

// threads

typedef int (*Runstep)(struct Thr& thr);

struct Thr {
  enum State { Wait, Start, Stop, Stopped, Exit };
  State m_state;
  int m_no;
  Runstep m_runstep;
  int m_ret;
  NdbMutex* m_mutex;
  NdbCondition* m_cond;
  NdbThread* m_thread;
  void* m_status;
  Ndb* m_ndb;
  NdbConnection* m_con;
  NdbScanOperation* m_scanop;
  NdbIndexScanOperation* m_indexscanop;
  //
  Thr(int no);
  ~Thr();
  int run();
  void start(Runstep runstep);
  void stop();
  void stopped();
  void lock() { NdbMutex_Lock(m_mutex); }
  void unlock() { NdbMutex_Unlock(m_mutex); }
  void wait() { NdbCondition_Wait(m_cond, m_mutex); }
  void signal() { NdbCondition_Signal(m_cond); }
  void exit();
  void join() { NdbThread_WaitFor(m_thread, &m_status); }
};

static NdbOut&
operator<<(NdbOut& out, const Thr& thr) {
  out << "thr " << thr.m_no;
  return out;
}

extern "C" { static void* runthread(void* arg); }

Thr::Thr(int no)
{
  m_state = Wait;
  m_no = no;
  m_runstep = 0;
  m_ret = 0;
  m_mutex = NdbMutex_Create();
  m_cond = NdbCondition_Create();
  require(m_mutex != 0 && m_cond != 0);
  const unsigned stacksize = 256 * 1024;
  const NDB_THREAD_PRIO prio = NDB_THREAD_PRIO_LOW;
  m_thread = NdbThread_Create(runthread, (void**)this, stacksize, "me", prio);
  if (m_thread == 0) {
    DBG("create thread failed: errno=" << errno);
    m_ret = -1;
  }
  m_status = 0;
  m_ndb = 0;
  m_con = 0;
  m_scanop = 0;
  m_indexscanop = 0;
}

Thr::~Thr()
{
  if (m_thread != 0)
    NdbThread_Destroy(&m_thread);
  if (m_cond != 0)
    NdbCondition_Destroy(m_cond);
  if (m_mutex != 0)
    NdbMutex_Destroy(m_mutex);
}

static void*
runthread(void* arg) {
  Thr& thr = *(Thr*)arg;
  thr.run();
  return 0;
}

int
Thr::run()
{
  DBG(*this << " run");
  while (true) {
    lock();
    while (m_state != Start && m_state != Exit) {
      wait();
    }
    if (m_state == Exit) {
      DBG(*this << " exit");
      unlock();
      break;
    }
    m_ret = (*m_runstep)(*this);
    m_state = Stopped;
    signal();
    unlock();
    if (m_ret != 0) {
      DBG(*this << " error exit");
      break;
    }
  }
  delete m_ndb;
  m_ndb = 0;
  return 0;
}

void
Thr::start(Runstep runstep)
{
  lock();
  m_state = Start;
  m_runstep = runstep;
  signal();
  unlock();
}

void
Thr::stopped()
{
  lock();
  while (m_state != Stopped) {
    wait();
  }
  m_state = Wait;
  unlock();
}

void
Thr::exit()
{
  lock();
  m_state = Exit;
  signal();
  unlock();
}

// general

static int
runstep_connect(Thr& thr)
{
  Ndb* ndb = thr.m_ndb = new Ndb(g_cluster_connection, "TEST_DB");
  CHN(ndb, ndb->init() == 0);
  CHN(ndb, ndb->waitUntilReady() == 0);
  DBG(thr << " connected");
  return 0;
}

static int
runstep_starttx(Thr& thr)
{
  Ndb* ndb = thr.m_ndb;
  require(ndb != 0);
  CHN(ndb, (thr.m_con = ndb->startTransaction()) != 0);
  DBG("thr " << thr.m_no << " tx started");
  return 0;
}

/*
 * WL1822 flush locks
 *
 * Table T with 3 tuples X, Y, Z.
 * Two transactions (* = lock wait).
 *
 * - tx1 reads and locks Z
 * - tx2 scans X, Y, *Z
 * - tx2 returns X, Y before lock wait on Z
 * - tx1 reads and locks *X
 * - api asks for next tx2 result
 * - LQH unlocks X via ACC or TUX [*]
 * - tx1 gets lock on X
 * - tx1 returns X to api
 * - api commits tx1
 * - tx2 gets lock on Z
 * - tx2 returns Z to api
 *
 * The point is deadlock is avoided due to [*].
 * The test is for 1 db node and 1 fragment table.
 */

static char wl1822_scantx = 0;

static const Uint32 wl1822_valA[3] = { 0, 1, 2 };
static const Uint32 wl1822_valB[3] = { 3, 4, 5 };

static Uint32 wl1822_bufA = ~0;
static Uint32 wl1822_bufB = ~0;

// map scan row to key (A) and reverse
static unsigned wl1822_r2k[3] = { 0, 0, 0 };
static unsigned wl1822_k2r[3] = { 0, 0, 0 };

static int
wl1822_createtable(Thr& thr)
{
  Ndb* ndb = thr.m_ndb;
  require(ndb != 0);
  NdbDictionary::Dictionary* dic = ndb->getDictionary();
  // drop T
  if (dic->getTable(g_opt.m_tname) != 0)
    CHN(dic, dic->dropTable(g_opt.m_tname) == 0);
  // create T
  NdbDictionary::Table tab(g_opt.m_tname);
  tab.setFragmentType(NdbDictionary::Object::FragAllSmall);
  { NdbDictionary::Column col("A");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  { NdbDictionary::Column col("B");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(false);
    tab.addColumn(col);
  }
  CHN(dic, dic->createTable(tab) == 0);
  // create X
  NdbDictionary::Index ind(g_opt.m_xname);
  ind.setTable(g_opt.m_tname);
  ind.setType(NdbDictionary::Index::OrderedIndex);
  ind.setLogging(false);
  ind.addColumn("B");
  CHN(dic, dic->createIndex(ind) == 0);
  DBG("created " << g_opt.m_tname << ", " << g_opt.m_xname);
  return 0;
}

static int
wl1822_insertrows(Thr& thr)
{
  // insert X, Y, Z
  Ndb* ndb = thr.m_ndb;
  require(ndb != 0);
  NdbConnection* con;
  NdbOperation* op;
  for (unsigned k = 0; k < 3; k++) {
    CHN(ndb, (con = ndb->startTransaction()) != 0);
    CHN(con, (op = con->getNdbOperation(g_opt.m_tname)) != 0);
    CHN(op, op->insertTuple() == 0);
    CHN(op, op->equal("A", (char*)&wl1822_valA[k]) == 0);
    CHN(op, op->setValue("B", (char*)&wl1822_valB[k]) == 0);
    CHN(con, con->execute(Commit) == 0);
    ndb->closeTransaction(con);
  }
  DBG("inserted X, Y, Z");
  return 0;
}

static int
wl1822_getscanorder(Thr& thr)
{
  // cheat, table order happens to be key order in my test
  wl1822_r2k[0] = 0;
  wl1822_r2k[1] = 1;
  wl1822_r2k[2] = 2;
  wl1822_k2r[0] = 0;
  wl1822_k2r[1] = 1;
  wl1822_k2r[2] = 2;
  DBG("scan order determined");
  return 0;
}

static int
wl1822_tx1_readZ(Thr& thr)
{
  // tx1 read Z with exclusive lock
  NdbConnection* con = thr.m_con;
  require(con != 0);
  NdbOperation* op;
  CHN(con, (op = con->getNdbOperation(g_opt.m_tname)) != 0);
  CHN(op, op->readTupleExclusive() == 0);
  CHN(op, op->equal("A", wl1822_valA[wl1822_r2k[2]]) == 0);
  wl1822_bufB = ~0;
  CHN(op, op->getValue("B", (char*)&wl1822_bufB) != 0);
  CHN(con, con->execute(NoCommit) == 0);
  CHK(wl1822_bufB == wl1822_valB[wl1822_r2k[2]]);
  DBG("tx1 locked Z");
  return 0;
}

static int
wl1822_tx2_scanXY(Thr& thr)
{
  // tx2 scan X, Y with exclusive lock
  NdbConnection* con = thr.m_con;
  require(con != 0);
  NdbScanOperation* scanop = nullptr;
  NdbIndexScanOperation* indexscanop;

  if (wl1822_scantx == 't') {
    CHN(con, (scanop = thr.m_scanop = con->getNdbScanOperation(g_opt.m_tname)) != 0);
    DBG("tx2 scan exclusive " << g_opt.m_tname);
  }
  if (wl1822_scantx == 'x') {
    CHN(con, (scanop = thr.m_scanop = indexscanop = thr.m_indexscanop = con->getNdbIndexScanOperation(g_opt.m_xname, g_opt.m_tname)) != 0);
    DBG("tx2 scan exclusive " << g_opt.m_xname);
  }
  CHN(scanop, scanop->readTuplesExclusive(16) == 0);
  CHN(scanop, scanop->getValue("A", (char*)&wl1822_bufA) != 0);
  CHN(scanop, scanop->getValue("B", (char*)&wl1822_bufB) != 0);
  CHN(con, con->execute(NoCommit) == 0);
  unsigned row = 0;
  while (row < 2) {
    DBG("before row " << row);
    int ret;
    wl1822_bufA = wl1822_bufB = ~0;
    CHN(con, (ret = scanop->nextResult(true)) == 0);
    DBG("got row " << row << " a=" << wl1822_bufA << " b=" << wl1822_bufB);
    CHK(wl1822_bufA == wl1822_valA[wl1822_r2k[row]]);
    CHK(wl1822_bufB == wl1822_valB[wl1822_r2k[row]]);
    row++;
  }
  return 0;
}

static int
wl1822_tx1_readX_commit(Thr& thr)
{
  // tx1 read X with exclusive lock and commit
  NdbConnection* con = thr.m_con;
  require(con != 0);
  NdbOperation* op;
  CHN(con, (op = con->getNdbOperation(g_opt.m_tname)) != 0);
  CHN(op, op->readTupleExclusive() == 0);
  CHN(op, op->equal("A", wl1822_valA[wl1822_r2k[2]]) == 0);
  wl1822_bufB = ~0;
  CHN(op, op->getValue("B", (char*)&wl1822_bufB) != 0);
  CHN(con, con->execute(NoCommit) == 0);
  CHK(wl1822_bufB == wl1822_valB[wl1822_r2k[2]]);
  DBG("tx1 locked X");
  CHN(con, con->execute(Commit) == 0);
  DBG("tx1 commit");
  return 0;
}

static int
wl1822_tx2_scanZ_close(Thr& thr)
{
  // tx2 scan Z with exclusive lock and close scan
  Ndb* ndb = thr.m_ndb;
  NdbConnection* con = thr.m_con;
  NdbScanOperation* scanop = thr.m_scanop;
  require(ndb != 0 && con != 0 && scanop != 0);
  unsigned row = 2;
  while (true) {
    DBG("before row " << row);
    int ret;
    wl1822_bufA = wl1822_bufB = ~0;
    CHN(con, (ret = scanop->nextResult(true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    DBG("got row " << row << " a=" << wl1822_bufA << " b=" << wl1822_bufB);
    CHK(wl1822_bufA == wl1822_valA[wl1822_r2k[row]]);
    CHK(wl1822_bufB == wl1822_valB[wl1822_r2k[row]]);
    row++;
  }
  ndb->closeTransaction(con);
  CHK(row == 3);
  return 0;
}

// threads are synced between each step
static Runstep wl1822_step[][2] = {
  { runstep_connect, runstep_connect },
  { wl1822_createtable, 0 },
  { wl1822_insertrows, 0 },
  { wl1822_getscanorder, 0 },
  { runstep_starttx, runstep_starttx },
  { wl1822_tx1_readZ, 0 },
  { 0, wl1822_tx2_scanXY },
  { wl1822_tx1_readX_commit, wl1822_tx2_scanZ_close }
};
const unsigned wl1822_stepcount = sizeof(wl1822_step)/sizeof(wl1822_step[0]);

static int
wl1822_main(char scantx)
{
  wl1822_scantx = scantx;
  static const unsigned thrcount = 2;
  // create threads for tx1 and tx2
  Thr* thrlist[2];
  unsigned n;
  for (n = 0; n < thrcount; n++) {
    Thr& thr = *(thrlist[n] = new Thr(1 + n));
    CHK(thr.m_ret == 0);
  }
  // run the steps
  for (unsigned i = 0; i < wl1822_stepcount; i++) {
    DBG("step " << i << " start");
    for (n = 0; n < thrcount; n++) {
      Thr& thr = *thrlist[n];
      Runstep runstep = wl1822_step[i][n];
      if (runstep != 0)
        thr.start(runstep);
    }
    for (n = 0; n < thrcount; n++) {
      Thr& thr = *thrlist[n];
      Runstep runstep = wl1822_step[i][n];
      if (runstep != 0)
        thr.stopped();
    }
  }
  // delete threads
  for (n = 0; n < thrcount; n++) {
    Thr& thr = *thrlist[n];
    thr.exit();
    thr.join();
    delete &thr;
  }
  return 0;
}

int main(int argc, char** argv)
{
  ndb_init();
  if (ndbout_mutex == NULL)
    ndbout_mutex= NdbMutex_Create();
  while (++argv, --argc > 0) {
    const char* arg = argv[0];
    if (strcmp(arg, "-scan") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_scan = strdup(argv[0]);
        continue;
      }
    }
    printusage();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  g_cluster_connection= &con;
  
  if ((strchr(g_opt.m_scan, 't') != 0 && wl1822_main('t') == -1) ||
      (strchr(g_opt.m_scan, 'x') != 0 && wl1822_main('x') == -1))
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  return NDBT_ProgramExit(NDBT_OK);
}

// vim: set sw=2 et:
