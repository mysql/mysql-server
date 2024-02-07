/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

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

#include <ndb_global.h>
#include <ndb_opts.h>
#include <ndb_version.h>
#include <NdbApi.hpp>
#include <NdbTest.hpp>
#include "util/require.h"

#include <NdbHost.h>
#include <NdbSleep.h>
#include <ndb_rand.h>

// version >= 5.1 required

#if !defined(min) || !defined(max)
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

/*
 * Test composite operations on same PK via events.  The merge of event
 * data can happen in 2 places:
 *
 * 1) In TUP at commit, the detached triggers report a single composite
 * operation and its post/pre data
 *
 * 2) In event API version >= 5.1 separate commits within same GCI are
 * optionally merged.  This is required to read blob data via NdbBlob.
 *
 * In this test merge is on by default.
 *
 * Option --separate-events disables GCI merge and implies --no-blobs.
 * This is used to test basic events functionality.
 *
 * Option --no-blobs omits blob attributes.  This is used to test GCI
 * merge without getting into blob bugs.
 *
 * Option --no-multiops allows 1 operation per commit.  This avoids TUP
 * and blob multi-operation bugs.
 *
 * There are other -no-* options, each added to isolate a specific bug.
 *
 * There are 5 ways (ignoring NUL operand) to compose 2 ops:
 *
 * INS o DEL = NUL
 * INS o UPD = INS
 * DEL o INS = UPD
 * UPD o DEL = DEL
 * UPD o UPD = UPD
 *
 * Event merge in NDB API handles idempotent INS o INS and DEL o DEL
 * which are possible on NF (node failure).  This test does not handle
 * them when --separate-events is used.
 */

struct Opts {
  bool abort_on_error;
  int blob_version;
  int loglevel;
  uint loop;
  uint maxops;
  uint maxpk;
  bool no_blobs;
  bool no_implicit_nulls;
  bool no_missing_update;
  bool no_multiops;
  bool no_nulls;
  bool one_blob;
  const char *opstring;
  uint seed;
  int maxtab;
  bool separate_events;
  uint tweak;  // whatever's useful
  bool use_table;
};

static Opts g_opts;
static const uint g_maxpk = 1000;
static const uint g_maxtab = 100;
static const uint g_maxopstringpart = 100;
static const char *g_opstringpart[g_maxopstringpart];
static uint g_opstringparts = 0;
static uint g_loop = 0;

static Ndb_cluster_connection *g_ncc = 0;
static Ndb *g_ndb = 0;
static NdbDictionary::Dictionary *g_dic = 0;
static NdbTransaction *g_con = 0;
static NdbOperation *g_op = 0;
static NdbScanOperation *g_scan_op = 0;

static const uint g_charlen = 5;
static const char *g_charval = "abcdefgh";
static const char *g_csname = "latin1_swedish_ci";

static uint g_blobinlinesize = 256;
static uint g_blobpartsize = 2000;
static const uint g_maxblobsize = 100000;

static NdbEventOperation *g_evt_op = 0;
static NdbBlob *g_bh = 0;

static uint urandom() {
  uint r = (uint)ndb_rand();
  return r;
}

static uint urandom(uint m) {
  if (m == 0) return 0;
  uint r = urandom();
  r = r % m;
  return r;
}

static bool urandom(uint per, uint cent) { return urandom(cent) < per; }

static int &g_loglevel = g_opts.loglevel;  // default log level

#define chkdb(x)                                             \
  do {                                                       \
    if (x) break;                                            \
    ndbout << "line " << __LINE__ << " FAIL " << #x << endl; \
    errdb();                                                 \
    if (g_opts.abort_on_error) abort();                      \
    return -1;                                               \
  } while (0)

#define chkrc(x)                                             \
  do {                                                       \
    if (x) break;                                            \
    ndbout << "line " << __LINE__ << " FAIL " << #x << endl; \
    if (g_opts.abort_on_error) abort();                      \
    return -1;                                               \
  } while (0)

#define reqrc(x)                                               \
  do {                                                         \
    if (x) break;                                              \
    ndbout << "line " << __LINE__ << " ASSERT " << #x << endl; \
    abort();                                                   \
  } while (0)

#define ll0(x)                 \
  do {                         \
    if (g_loglevel < 0) break; \
    ndbout << x << endl;       \
  } while (0)

#define ll1(x)                 \
  do {                         \
    if (g_loglevel < 1) break; \
    ndbout << x << endl;       \
  } while (0)

#define ll2(x)                 \
  do {                         \
    if (g_loglevel < 2) break; \
    ndbout << x << endl;       \
  } while (0)

#define ll3(x)                 \
  do {                         \
    if (g_loglevel < 3) break; \
    ndbout << x << endl;       \
  } while (0)

static void errdb() {
  uint any = 0;
  // g_ncc return no error...
  if (g_ndb != 0) {
    const NdbError &e = g_ndb->getNdbError();
    if (e.code != 0) ll0(++any << " ndb: error " << e);
  }
  if (g_dic != 0) {
    const NdbError &e = g_dic->getNdbError();
    if (e.code != 0) ll0(++any << " dic: error " << e);
  }
  if (g_con != 0) {
    const NdbError &e = g_con->getNdbError();
    if (e.code != 0) ll0(++any << " con: error " << e);
  }
  if (g_op != 0) {
    const NdbError &e = g_op->getNdbError();
    if (e.code != 0) ll0(++any << " op: error " << e);
  }
  if (g_scan_op != 0) {
    const NdbError &e = g_scan_op->getNdbError();
    if (e.code != 0) ll0(++any << " scan_op: error " << e);
  }
  if (g_evt_op != 0) {
    const NdbError &e = g_evt_op->getNdbError();
    if (e.code != 0) ll0(++any << " evt_op: error " << e);
  }
  if (g_bh != 0) {
    const NdbError &e = g_bh->getNdbError();
    if (e.code != 0) ll0(++any << " bh: error " << e);
  }
  if (!any) ll0("unknown db error");
}

struct Col {
  uint no;
  const char *name;
  NdbDictionary::Column::Type type;
  bool pk;
  bool nullable;
  uint length;
  uint size;
  uint inlinesize;
  uint partsize;
  uint stripesize;
  bool isblob() const {
    return type == NdbDictionary::Column::Text ||
           type == NdbDictionary::Column::Blob;
  }
};

// put var* pk first
static const Col g_col[] = {
    {0, "pk2", NdbDictionary::Column::Varchar, true, false, g_charlen,
     1 + g_charlen, 0, 0, 0},
    {1, "seq", NdbDictionary::Column::Unsigned, false, true, 1, 4, 0, 0, 0},
    {2, "pk1", NdbDictionary::Column::Unsigned, true, false, 1, 4, 0, 0, 0},
    {3, "cc1", NdbDictionary::Column::Char, false, true, g_charlen, g_charlen,
     0, 0, 0},
    {4, "tx1", NdbDictionary::Column::Text, false, true, 0, 0, g_blobinlinesize,
     g_blobpartsize, 0},  // V2 distribution
    {5, "tx2", NdbDictionary::Column::Text, false, true, 0, 0, g_blobinlinesize,
     g_blobpartsize, 4},
    {6, "bl1", NdbDictionary::Column::Blob,  // tinyblob
     false, true, 0, 0, g_blobinlinesize, 0, 0}};

static const uint g_maxcol = sizeof(g_col) / sizeof(g_col[0]);
static const uint g_blobcols = 3;

static uint ncol() {
  uint n = g_maxcol;
  if (g_opts.no_blobs)
    n -= g_blobcols;
  else if (g_opts.one_blob)
    n -= (g_blobcols - 2);
  return n;
}

static const Col &getcol(uint i) {
  if (i < ncol()) return g_col[i];
  require(false);
  return g_col[0];
}

static const Col &getcol(const char *name) {
  uint i;
  for (i = 0; i < ncol(); i++)
    if (strcmp(g_col[i].name, name) == 0) break;
  return getcol(i);
}

struct Tab {
  char tabname[20];
  const Col *col;
  const NdbDictionary::Table *tab;
  char evtname[20];
  explicit Tab(uint idx) : col(g_col), tab(nullptr) {
    sprintf(tabname, "tem%d", idx);
    sprintf(evtname, "tem%dev", idx);
  }
};

static Tab *g_tablst[g_maxtab];

static uint maxtab() { return g_opts.maxtab; }

static Tab &tab(uint i) {
  require(i < maxtab() && g_tablst[i] != 0);
  return *g_tablst[i];
}

static int createtable(Tab &t) {
  ll2("createtable: " << t.tabname);
  t.tab = 0;
  NdbDictionary::Table tab(t.tabname);
  tab.setLogging(false);
  CHARSET_INFO *cs;
  chkrc((cs = get_charset_by_name(g_csname, MYF(0))) != 0);
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = t.col[i];
    NdbDictionary::Column col(c.name);
    col.setType(c.type);
    col.setPrimaryKey(c.pk);
    col.setNullable(c.nullable);
    switch (c.type) {
      case NdbDictionary::Column::Unsigned:
        break;
      case NdbDictionary::Column::Char:
      case NdbDictionary::Column::Varchar:
        col.setLength(c.length);
        col.setCharset(cs);
        break;
      case NdbDictionary::Column::Text:
        col.setBlobVersion(g_opts.blob_version);
        col.setInlineSize(c.inlinesize);
        col.setPartSize(c.partsize);
        col.setStripeSize(g_opts.blob_version == 1 ? 4 : c.stripesize);
        col.setCharset(cs);
        break;
      case NdbDictionary::Column::Blob:
        col.setBlobVersion(g_opts.blob_version);
        col.setInlineSize(c.inlinesize);
        col.setPartSize(c.partsize);
        col.setStripeSize(g_opts.blob_version == 1 ? 4 : c.stripesize);
        break;
      default:
        require(false);
        break;
    }
    tab.addColumn(col);
  }
  g_dic = g_ndb->getDictionary();
  if (!g_opts.use_table) {
    if (g_dic->getTable(t.tabname) != 0)
      chkdb(g_dic->dropTable(t.tabname) == 0);
    chkdb(g_dic->createTable(tab) == 0);
  }
  chkdb((t.tab = g_dic->getTable(t.tabname)) != 0);
  g_dic = 0;
  if (!g_opts.use_table) {
    // extra row for GCI probe
    chkdb((g_con = g_ndb->startTransaction()) != 0);
    chkdb((g_op = g_con->getNdbOperation(t.tabname)) != 0);
    chkdb(g_op->insertTuple() == 0);
    Uint32 pk1;
    char pk2[1 + g_charlen + 1];
    pk1 = g_maxpk;
    sprintf(pk2 + 1, "%-u", pk1);
    *(uchar *)pk2 = (uchar)(strlen(pk2 + 1));
    chkdb(g_op->equal("pk1", (char *)&pk1) == 0);
    chkdb(g_op->equal("pk2", (char *)&pk2[0]) == 0);
    chkdb(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);
    g_op = 0;
    g_con = 0;
  }
  return 0;
}

static int createtables() {
  ll1("createtables");
  for (uint i = 0; i < maxtab(); i++) chkrc(createtable(tab(i)) == 0);
  return 0;
}

static int droptable(Tab &t) {
  ll2("droptable: " << t.tabname);
  if (!g_opts.use_table) {
    g_dic = g_ndb->getDictionary();
    chkdb(g_dic->dropTable(t.tabname) == 0);
    t.tab = 0;
    g_dic = 0;
  }
  return 0;
}

static int droptables() {
  ll1("droptables");
  for (uint i = 0; i < maxtab(); i++) chkrc(droptable(tab(i)) == 0);
  return 0;
}

static int createevent(Tab &t) {
  ll2("createevent: " << t.evtname);
  g_dic = g_ndb->getDictionary();
  NdbDictionary::Event evt(t.evtname);
  require(t.tab != 0);
  evt.setTable(*t.tab);
  evt.addTableEvent(NdbDictionary::Event::TE_ALL);
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = g_col[i];
    evt.addEventColumn(c.name);
  }
  const NdbDictionary::Event::EventReport er = NdbDictionary::Event::ER_UPDATED;
  evt.setReport(er);
  evt.mergeEvents(!g_opts.separate_events);
#if 0  // XXX random bugs
  if (g_dic->getEvent(t.evtname) != 0)
    chkdb(g_dic->dropEvent(t.evtname) == 0);
#else
  (void)g_dic->dropEvent(t.evtname);
  chkdb(g_dic->createEvent(evt) == 0);
#endif
  NdbDictionary::Event_ptr ev(g_dic->getEvent(t.evtname));
  chkdb(ev != nullptr);
  chkrc(ev->getReport() == er);
  chkrc((ev->getReportOptions() & er) == er);
  chkrc(ev->getDurability() == NdbDictionary::Event::ED_PERMANENT);
  g_dic = 0;
  return 0;
}

static int createevents() {
  ll1("createevents");
  for (uint i = 0; i < maxtab(); i++) chkrc(createevent(tab(i)) == 0);
  return 0;
}

static int dropevent(Tab &t, bool force = false) {
  ll2("dropevent: " << t.evtname);
  g_dic = g_ndb->getDictionary();
  chkdb(g_dic->dropEvent(t.evtname) == 0 || force);
  g_dic = 0;
  return 0;
}

static int dropevents(bool force = false) {
  ll1("dropevents");
  for (uint i = 0; i < maxtab(); i++) {
    if (force && g_tablst[i] == 0) continue;
    chkrc(dropevent(tab(i), force) == 0 || force);
  }
  return 0;
}

struct Data {
  struct Txt {
    char *val;
    uint len;
  };
  union Ptr {
    Uint32 *u32;
    char *ch;
    uchar *uch;
    Txt *txt;
    void *v;
  };
  Uint32 pk1;
  char pk2[g_charlen + 1];
  Uint32 seq;
  char cc1[g_charlen + 1];
  Txt tx1;
  Txt tx2;
  Txt bl1;
  Ptr ptr[g_maxcol];
  int ind[g_maxcol];  // -1 = no data, 1 = NULL, 0 = not NULL
  uint noop;          // bit: omit in NdbOperation (implicit NULL INS or no UPD)
  uint ppeq;          // bit: post/pre data value equal in GCI data[0]/data[1]
  void init() {
    uint i;
    pk1 = 0;
    memset(pk2, 0, sizeof(pk2));
    seq = 0;
    memset(cc1, 0, sizeof(cc1));
    tx1.val = tx2.val = bl1.val = 0;
    tx1.len = tx2.len = bl1.len = 0;
    ptr[0].ch = pk2;
    ptr[1].u32 = &seq;
    ptr[2].u32 = &pk1;
    ptr[3].ch = cc1;
    ptr[4].txt = &tx1;
    ptr[5].txt = &tx2;
    ptr[6].txt = &bl1;
    for (i = 0; i < g_maxcol; i++) ind[i] = -1;
    noop = 0;
    ppeq = 0;
  }
  void freemem() {
    delete[] tx1.val;
    delete[] tx2.val;
    delete[] bl1.val;
    tx1.val = tx2.val = bl1.val = 0;
    tx1.len = tx2.len = bl1.len = 0;
  }
};

static int cmpcol(const Col &c, const Data &d1, const Data &d2) {
  uint i = c.no;
  if (d1.ind[i] != d2.ind[i]) return 1;
  if (d1.ind[i] == 0) {
    switch (c.type) {
      case NdbDictionary::Column::Unsigned:
        if (*d1.ptr[i].u32 != *d2.ptr[i].u32) return 1;
        break;
      case NdbDictionary::Column::Char:
        if (memcmp(d1.ptr[i].ch, d2.ptr[i].ch, c.size) != 0) return 1;
        break;
      case NdbDictionary::Column::Varchar: {
        uint l1 = d1.ptr[i].uch[0];
        uint l2 = d2.ptr[i].uch[0];
        if (l1 != l2) return 1;
        if (memcmp(d1.ptr[i].ch, d2.ptr[i].ch, l1) != 0) return 1;
      } break;
      case NdbDictionary::Column::Text:
      case NdbDictionary::Column::Blob: {
        const Data::Txt &t1 = *d1.ptr[i].txt;
        const Data::Txt &t2 = *d2.ptr[i].txt;
        if (t1.len != t2.len) return 1;
        if (memcmp(t1.val, t2.val, t1.len) != 0) return 1;
      } break;
      default:
        require(false);
        break;
    }
  }
  return 0;
}

static NdbOut &operator<<(NdbOut &out, const Data &d) {
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = getcol(i);
    out << (i == 0 ? "" : " ") << c.name;
    out << (!(d.noop & (1 << i)) ? "=" : ":");
    if (d.ind[i] == -1) continue;
    if (d.ind[i] == 1) {
      out << "NULL";
      continue;
    }
    switch (c.type) {
      case NdbDictionary::Column::Unsigned:
        out << *d.ptr[i].u32;
        break;
      case NdbDictionary::Column::Char: {
        char buf[g_charlen + 1];
        memcpy(buf, d.ptr[i].ch, g_charlen);
        uint n = g_charlen;
        while (1) {
          buf[n] = 0;
          if (n == 0 || buf[n - 1] != 0x20) break;
          n--;
        }
        out << "'" << buf << "'";
      } break;
      case NdbDictionary::Column::Varchar: {
        char buf[g_charlen + 1];
        uint l = d.ptr[i].uch[0];
        require(l <= g_charlen);
        memcpy(buf, &d.ptr[i].ch[1], l);
        buf[l] = 0;
        out << "'" << buf << "'";
      } break;
      case NdbDictionary::Column::Text:
      case NdbDictionary::Column::Blob: {
        Data::Txt &txt = *d.ptr[i].txt;
        bool first = true;
        uint j = 0;
        while (j < txt.len) {
          char c[2];
          c[0] = txt.val[j++];
          c[1] = 0;
          uint m = 1;
          while (j < txt.len && txt.val[j] == c[0]) j++, m++;
          if (!first) out << "+";
          first = false;
          out << m << c;
        }
      } break;
      default:
        require(false);
        break;
    }
  }
  return out;
}

// some random os may define these
#undef UNDEF
#undef INS
#undef DEL
#undef UPD
#undef NUL

static const uint g_optypes = 3;  // real ops 0-2

/*
 * Represents single or composite operation or received event.  The
 * post/pre data is either computed here for operations or received from
 * the event.
 */
struct Op {  // single or composite
  enum Kind { OP = 1, EV = 2 };
  enum Type { UNDEF = -1, INS, DEL, UPD, NUL };
  Kind kind;
  Type type;
  Op *next_op;    // within one commit
  Op *next_com;   // next commit chain
  Op *next_gci;   // groups commit chains (unless --separate-events)
  Op *next_ev;    // next event
  Op *next_free;  // free list
  bool free;      // on free list
  uint num_op;
  uint num_com;
  Data data[2];  // 0-post 1-pre
  bool match;    // matched to event
  Uint64 gci;    // defined for com op and event
  void init(Kind a_kind, Type a_type = UNDEF) {
    kind = a_kind;
    require(kind == OP || kind == EV);
    type = a_type;
    next_op = next_com = next_gci = next_ev = next_free = 0;
    free = false;
    num_op = num_com = 0;
    data[0].init();
    data[1].init();
    match = false;
    gci = 0;
  }
  void freemem() {
    data[0].freemem();
    data[1].freemem();
  }
};

static NdbOut &operator<<(NdbOut &out, Op::Type optype) {
  switch (optype) {
    case Op::INS:
      out << "INS";
      break;
    case Op::DEL:
      out << "DEL";
      break;
    case Op::UPD:
      out << "UPD";
      break;
    case Op::NUL:
      out << "NUL";
      break;
    default:
      out << (int)optype;
      break;
  }
  return out;
}

static NdbOut &operator<<(NdbOut &out, const Op &op) {
  out << op.type;
  out << " " << op.data[0];
  out << " [" << op.data[1] << "]";
  if (op.gci != 0) out << " gci:" << op.gci;
  return out;
}

static int seteventtype(Op *ev, NdbDictionary::Event::TableEvent te) {
  Op::Type optype = Op::UNDEF;
  switch (te) {
    case NdbDictionary::Event::TE_INSERT:
      optype = Op::INS;
      break;
    case NdbDictionary::Event::TE_DELETE:
      optype = Op::DEL;
      break;
    case NdbDictionary::Event::TE_UPDATE:
      optype = Op::UPD;
      break;
    default:
      ll0("EVT: " << *ev << ": bad event type " << hex << (uint)te);
      return -1;
  }
  ev->type = optype;
  return 0;
}

struct Counter {  // debug aid
  const char *name;
  uint count;
  Counter(const char *a_name) : name(a_name), count(0) {}
  friend class NdbOut &operator<<(NdbOut &out, const Counter &counter) {
    out << counter.name << "(" << counter.count << ")";
    return out;
  }
  operator uint() { return count; }
  Counter operator++(int) {
    ll3(*this << "++");
    Counter tmp = *this;
    count++;
    return tmp;
  }
  Counter operator--(int) {
    ll3(*this << "--");
    require(count != 0);
    Counter tmp = *this;
    count--;
    return tmp;
  }
};

static Op *g_opfree = 0;
static uint g_freeops = 0;
static uint g_usedops = 0;
static uint g_gciops = 0;
static uint g_maxcom = 10;  // max ops per commit
static uint g_seq = 0;
static Op *g_rec_ev;
static uint g_num_ev = 0;

static const uint g_maxgcis = 500;  // max GCIs seen during 1 loop

// operation data per table and each loop
struct Run : public Tab {
  bool skip;  // no ops in current loop
  NdbEventOperation *evt_op;
  uint gcicnt;  // number of CGIs seen in current loop
  Uint64 gcinum[g_maxgcis];
  Uint32 gcievtypes[g_maxgcis][2];  // 0-getGCIEventOperations 1-nextEvent
  uint tableops;                    // real table ops in this loop
  uint blobops;                     // approx blob part ops in this loop
  uint gciops;           // commit chains or (after mergeops) gci chains
  Op *pk_op[g_maxpk];    // GCI chain of ops per PK
  Op *pk_ev[g_maxpk];    // events per PK
  uint ev_pos[g_maxpk];  // counts events
  NdbRecAttr *ev_ra[2][g_maxcol];  // 0-post 1-pre
  NdbBlob *ev_bh[2][g_maxcol];     // 0-post 1-pre
  Run(uint idx) : Tab(idx) { reset(); }
  void reset() {
    int i, j;
    skip = false;
    evt_op = 0;
    gcicnt = 0;
    for (i = 0; i < (int)g_maxgcis; i++) {
      gcinum[i] = (Uint64)0;
      gcievtypes[i][0] = gcievtypes[i][1] = (Uint32)0;
    }
    tableops = 0;
    blobops = 0;
    gciops = 0;
    for (i = 0; i < (int)g_maxpk; i++) {
      pk_op[i] = 0;
      pk_ev[i] = 0;
      ev_pos[i] = 0;
    }
    for (j = 0; i < 2; j++) {
      for (i = 0; i < (int)g_maxcol; i++) {
        ev_ra[j][i] = 0;
        ev_bh[j][i] = 0;
      }
    }
  }
  int addgci(Uint64 gci) {
    require(gcicnt < g_maxgcis);
    chkrc(gcicnt == 0 || gcinum[gcicnt - 1] < gci);
    gcinum[gcicnt++] = gci;
    return 0;
  }
  void addevtypes(Uint64 gci, Uint32 evtypes, uint i) {
    require(gcicnt != 0);
    require(gci == gcinum[gcicnt - 1]);
    require(evtypes != 0);
    require(i < 2);
    gcievtypes[gcicnt - 1][i] |= evtypes;
  }
};

static Run *g_runlst[g_maxtab];

static uint maxrun() { return maxtab(); }

static Run &run(uint i) {
  require(i < maxrun() && g_runlst[i] != 0);
  return *g_runlst[i];
}

static void initrun() {
  uint i;
  for (i = 0; i < maxrun(); i++) g_tablst[i] = g_runlst[i] = new Run(i);
}

static Op *getop(Op::Kind a_kind, Op::Type a_type = Op::UNDEF) {
  if (g_opfree == 0) {
    Op *op = new Op;
    require(g_freeops == 0);
    require(op != 0);
    op->next_free = g_opfree;  // 0
    g_opfree = op;
    op->free = true;
    g_freeops++;
  }
  Op *op = g_opfree;
  g_opfree = op->next_free;
  require(g_freeops != 0);
  g_freeops--;
  g_usedops++;
  op->init(a_kind, a_type);
  op->free = false;
  ll3("getop: " << op);
  return op;
}

static void freeop(Op *op) {
  ll3("freeop: " << op);
  require(!op->free);
  op->freemem();
  op->free = true;
  op->next_free = g_opfree;
  g_opfree = op;
  g_freeops++;
  require(g_usedops != 0);
  g_usedops--;
}

static void resetmem(Run &r) {
  ll2("resetmem");
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) r.ev_pos[pk1] = 0;
  // leave g_seq
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    if (r.pk_op[pk1] != 0) {
      Op *tot_op = r.pk_op[pk1];
      while (tot_op->next_gci != 0) {
        Op *gci_op = tot_op->next_gci;
        while (gci_op->next_com != 0) {
          Op *com_op = gci_op->next_com;
          while (com_op->next_op != 0) {
            Op *op = com_op->next_op;
            com_op->next_op = op->next_op;
            freeop(op);
          }
          gci_op->next_com = com_op->next_com;
          freeop(com_op);
        }
        tot_op->next_gci = gci_op->next_gci;
        freeop(gci_op);
      }
      freeop(tot_op);
      r.pk_op[pk1] = 0;
    }
    if (r.pk_ev[pk1] != 0) {
      Op *tot_op = r.pk_ev[pk1];
      while (tot_op->next_ev != 0) {
        Op *ev = tot_op->next_ev;
        tot_op->next_ev = ev->next_ev;
        freeop(ev);
      }
      freeop(tot_op);
      r.pk_ev[pk1] = 0;
    }
  }
  r.reset();
}

static void resetmem() {
  if (g_rec_ev != 0) {
    freeop(g_rec_ev);
    g_rec_ev = 0;
  }
  for (uint i = 0; i < maxrun(); i++) resetmem(run(i));
  require(g_usedops == 0);
  g_gciops = g_num_ev = 0;
}

static void deleteops()  // for memleak checks
{
  while (g_opfree != 0) {
    Op *tmp_op = g_opfree;
    g_opfree = g_opfree->next_free;
    delete tmp_op;
    g_freeops--;
  }
  require(g_freeops == 0);
}

struct Comp {
  Op::Type t1, t2, t3;
};

static Comp g_comp[] = {{Op::INS, Op::DEL, Op::NUL},
                        {Op::INS, Op::UPD, Op::INS},
                        {Op::DEL, Op::INS, Op::UPD},
                        {Op::UPD, Op::DEL, Op::DEL},
                        {Op::UPD, Op::UPD, Op::UPD}};

static const uint g_ncomp = sizeof(g_comp) / sizeof(g_comp[0]);

static int checkop(const Op *op, Uint32 &pk1) {
  Op::Type optype = op->type;
  require(optype != Op::UNDEF);
  if (optype == Op::NUL) return 0;
  chkrc(optype == Op::INS || optype == Op::DEL || optype == Op::UPD);
  const Data &d0 = op->data[0];
  const Data &d1 = op->data[1];
  {
    const Col &c = getcol("pk1");
    chkrc(d0.ind[c.no] == 0);
    pk1 = d0.pk1;
    chkrc(pk1 < g_opts.maxpk);
  }
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = getcol(i);
    const int ind0 = d0.ind[i];
    const int ind1 = d1.ind[i];
    // the rules are the rules..
    if (c.pk) {
      chkrc(ind0 == 0);                          // always PK in post data
      if (optype == Op::INS) chkrc(ind1 == -1);  // no PK in pre data
      if (optype == Op::DEL)
        chkrc(ind1 == 0);  // always PK in pre data (note change from 6.3.23)
      if (optype == Op::UPD) chkrc(ind1 == 0);  // always PK in pre data
    }
    if (!c.pk) {
      if (optype == Op::INS) chkrc(ind0 >= 0 && ind1 == -1);
      if (optype == Op::DEL)
        chkrc(ind0 == -1 && ind1 >= 0);  // always non-PK in pre data
      if (optype == Op::UPD)
        chkrc(ind0 == -1 || ind1 >= 0);  // update must have pre data
    }
    if (!c.nullable) {
      chkrc(ind0 <= 0 && ind1 <= 0);
    }
    if (c.isblob()) {
      // blob values must be from allowed chars
      int j;
      for (j = 0; j < 2; j++) {
        const Data &d = op->data[j];
        if (d.ind[i] == 0) {
          const Data::Txt &txt = *d.ptr[i].txt;
          int k;
          for (k = 0; k < (int)txt.len; k++) {
            chkrc(strchr(g_charval, txt.val[k]) != 0);
          }
        }
      }
    }
  }
  return 0;
}

static Comp *comptype(Op::Type t1, Op::Type t2)  // only non-NUL
{
  uint i;
  for (i = 0; i < g_ncomp; i++)
    if (g_comp[i].t1 == t1 && g_comp[i].t2 == t2) return &g_comp[i];
  return 0;
}

static void copycol(const Col &c, const Data &d1, Data &d3) {
  uint i = c.no;
  if ((d3.ind[i] = d1.ind[i]) == 0) {
    if (!c.isblob()) {
      memmove(d3.ptr[i].v, d1.ptr[i].v, c.size);
    } else {
      Data::Txt &t1 = *d1.ptr[i].txt;
      Data::Txt &t3 = *d3.ptr[i].txt;
      delete[] t3.val;
      t3.val = new char[t1.len];
      t3.len = t1.len;
      memcpy(t3.val, t1.val, t1.len);
    }
  }
}

static void copydata(const Data &d1, Data &d3, bool pk, bool nonpk) {
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = g_col[i];
    if ((c.pk && pk) || (!c.pk && nonpk)) copycol(c, d1, d3);
  }
}

static void compdata(const Data &d1, const Data &d2, Data &d3, bool pk,
                     bool nonpk) {
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = g_col[i];
    if ((c.pk && pk) || (!c.pk && nonpk)) {
      const Data *d = 0;
      if (d1.ind[i] == -1 && d2.ind[i] == -1)
        d3.ind[i] = -1;
      else if (d1.ind[i] == -1 && d2.ind[i] != -1)
        d = &d2;
      else if (d1.ind[i] != -1 && d2.ind[i] == -1)
        d = &d1;
      else
        d = &d2;
      if (d != 0) copycol(c, *d, d3);
    }
  }
}

static void copyop(const Op *op1, Op *op3) {
  op3->type = op1->type;
  copydata(op1->data[0], op3->data[0], true, true);
  copydata(op1->data[1], op3->data[1], true, true);
  op3->gci = op1->gci;
  Uint32 pk1_tmp;
  reqrc(checkop(op3, pk1_tmp) == 0);
}

static int compop(const Op *op1, const Op *op2, Op *op3)  // op1 o op2 = op3
{
  require(op1->type != Op::UNDEF && op2->type != Op::UNDEF);
  Comp *comp;
  if (op2->type == Op::NUL) {
    copyop(op1, op3);
    return 0;
  }
  if (op1->type == Op::NUL) {
    copyop(op2, op3);
    return 0;
  }
  Op::Kind kind = op1->kind == Op::OP && op2->kind == Op::OP ? Op::OP : Op::EV;
  Op *res_op = getop(kind);
  chkrc((comp = comptype(op1->type, op2->type)) != 0);
  res_op->type = comp->t3;
  if (res_op->type == Op::INS) {
    // INS o UPD
    compdata(op1->data[0], op2->data[0], res_op->data[0], true, true);
    // pre = undef
  }
  if (res_op->type == Op::DEL) {
    // UPD o DEL
    copydata(op2->data[0], res_op->data[0], true, false);  // PK only
    copydata(op1->data[1], res_op->data[1], true, true);   // PK + non-PK
  }
  if (res_op->type == Op::UPD && op1->type == Op::DEL) {
    // DEL o INS
    copydata(op2->data[0], res_op->data[0], true, true);
    copydata(op1->data[0], res_op->data[1], true, false);  // PK only
    copydata(op1->data[1], res_op->data[1], true, true);   // PK + non-PK
  }
  if (res_op->type == Op::UPD && op1->type == Op::UPD) {
    // UPD o UPD
    compdata(op1->data[0], op2->data[0], res_op->data[0], true, true);
    compdata(op2->data[1], op1->data[1], res_op->data[1], true, true);
  }
  require(op1->gci == op2->gci);
  res_op->gci = op2->gci;
  Uint32 pk1_tmp;
  reqrc(checkop(res_op, pk1_tmp) == 0);
  copyop(res_op, op3);
  freeop(res_op);
  return 0;
}

static int createeventop(Run &r) {
  ll2("createeventop: " << r.tabname);
  chkdb((r.evt_op = g_ndb->createEventOperation(r.evtname)) != 0);
  r.evt_op->mergeEvents(!g_opts.separate_events);  // not yet inherited
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = g_col[i];
    Data(&d)[2] = g_rec_ev->data;
    if (!c.isblob()) {
      chkdb((r.ev_ra[0][i] =
                 r.evt_op->getValue(c.name, (char *)d[0].ptr[i].v)) != 0);
      reqrc(r.ev_ra[0][i]->aRef() == (char *)d[0].ptr[i].v);  // uses ptr
      chkdb((r.ev_ra[1][i] =
                 r.evt_op->getPreValue(c.name, (char *)d[1].ptr[i].v)) != 0);
      reqrc(r.ev_ra[1][i]->aRef() == (char *)d[1].ptr[i].v);  // uses ptr
    } else {
      chkdb((r.ev_bh[0][i] = r.evt_op->getBlobHandle(c.name)) != 0);
      chkdb((r.ev_bh[1][i] = r.evt_op->getPreBlobHandle(c.name)) != 0);
    }
  }
  return 0;
}

static int createeventop() {
  ll1("createeventop");
  for (uint i = 0; i < maxrun(); i++) chkrc(createeventop(run(i)) == 0);
  return 0;
}

static int executeeventop(Run &r) {
  ll2("executeeventop: " << r.tabname);
  chkdb(r.evt_op->execute() == 0);
  return 0;
}

static int executeeventop() {
  ll1("executeeventop");
  for (uint i = 0; i < maxrun(); i++) chkrc(executeeventop(run(i)) == 0);
  return 0;
}

static int dropeventop(Run &r, bool force = false) {
  ll2("dropeventop: " << r.tabname);
  if (r.evt_op != 0) {
    chkdb(g_ndb->dropEventOperation(r.evt_op) == 0 || force);
    r.evt_op = 0;
  }
  return 0;
}

static int dropeventops(bool force = false) {
  ll1("dropeventops");
  for (uint i = 0; i < maxrun(); i++) {
    if (force && g_runlst[i] == 0) continue;
    chkrc(dropeventop(run(i), force) == 0 || force);
  }
  return 0;
}

// wait for event to be installed and for GCIs to pass
static int waitgci(uint ngci) {
  ll1("waitgci " << ngci);
  Uint64 gci[2];
  uint i = 0;
  while (1) {
    chkdb((g_con = g_ndb->startTransaction()) != 0);
    {                   // forced to exec a dummy op
      Tab &t = tab(0);  // use first table
      Uint32 pk1;
      char pk2[1 + g_charlen + 1];
      pk1 = g_maxpk;
      sprintf(pk2 + 1, "%-u", pk1);
      *(uchar *)pk2 = (uchar)(strlen(pk2 + 1));
      chkdb((g_op = g_con->getNdbOperation(t.tabname)) != 0);
      chkdb(g_op->readTuple() == 0);
      chkdb(g_op->equal("pk1", (char *)&pk1) == 0);
      chkdb(g_op->equal("pk2", (char *)&pk2[0]) == 0);
      chkdb(g_con->execute(Commit) == 0);
      g_op = 0;
    }
    g_con->getGCI(&gci[i]);
    g_ndb->closeTransaction(g_con);
    g_con = 0;
    if (i == 1 && gci[0] + ngci <= gci[1]) {
      ll1("waitgci: " << gci[0] << " " << gci[1]);
      break;
    }
    i = 1;
    NdbSleep_SecSleep(1);
  }
  return 0;
}

// scan table and set current tot_op for each pk1
static int scantable(Run &r) {
  ll2("scantable: " << r.tabname);
  NdbRecAttr *ra[g_maxcol];
  NdbBlob *bh[g_maxcol];
  Op *rec_op = getop(Op::OP);
  Data &d0 = rec_op->data[0];
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  chkdb((g_scan_op = g_con->getNdbScanOperation(r.tabname)) != 0);
  chkdb(g_scan_op->readTuples() == 0);
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = getcol(i);
    if (!c.isblob()) {
      chkdb((ra[i] = g_scan_op->getValue(c.name, (char *)d0.ptr[i].v)) != 0);
    } else {
      chkdb((bh[i] = g_scan_op->getBlobHandle(c.name)) != 0);
    }
  }
  chkdb(g_con->execute(NoCommit) == 0);
  int ret;
  while ((ret = g_scan_op->nextResult()) == 0) {
    Uint32 pk1 = d0.pk1;
    if (pk1 >= g_opts.maxpk) continue;
    rec_op->type = Op::INS;
    for (i = 0; i < ncol(); i++) {
      const Col &c = getcol(i);
      int ind;
      if (!c.isblob()) {
        ind = ra[i]->isNULL();
      } else {
        int ret;
        ret = bh[i]->getDefined(ind);
        require(ret == 0);
        if (ind == 0) {
          Data::Txt &txt = *d0.ptr[i].txt;
          Uint64 len64;
          ret = bh[i]->getLength(len64);
          require(ret == 0);
          txt.len = (uint)len64;
          delete[] txt.val;
          txt.val = new char[txt.len];
          memset(txt.val, 'X', txt.len);
          Uint32 len = txt.len;
          ret = bh[i]->readData(txt.val, len);
          require(ret == 0 && len == txt.len);
          // to see the data, have to execute...
          chkdb(g_con->execute(NoCommit) == 0);
          require(memchr(txt.val, 'X', txt.len) == 0);
        }
      }
      require(ind >= 0);
      d0.ind[i] = ind;
    }
    require(r.pk_op[pk1] == 0);
    Op *tot_op = r.pk_op[pk1] = getop(Op::OP);
    copyop(rec_op, tot_op);
    tot_op->type = Op::INS;
  }
  chkdb(ret == 1);
  g_ndb->closeTransaction(g_con);
  g_scan_op = 0;
  g_con = 0;
  freeop(rec_op);
  return 0;
}

static int scantable() {
  ll1("scantable");
  for (uint i = 0; i < maxrun(); i++) chkrc(scantable(run(i)) == 0);
  return 0;
}

static void makedata(const Col &c, Data &d, Uint32 pk1, Op::Type optype) {
  uint i = c.no;
  if (c.pk) {
    switch (c.type) {
      case NdbDictionary::Column::Unsigned: {
        Uint32 *p = d.ptr[i].u32;
        *p = pk1;
      } break;
      case NdbDictionary::Column::Char: {
        char *p = d.ptr[i].ch;
        sprintf(p, "%-*u", g_charlen, pk1);
      } break;
      case NdbDictionary::Column::Varchar: {
        char *p = &d.ptr[i].ch[1];
        sprintf(p, "%-u", pk1);
        uint len = pk1 % g_charlen;
        uint j = (uint)strlen(p);
        while (j < len) {
          p[j] = 'a' + j % 26;
          j++;
        }
        d.ptr[i].uch[0] = len;
      } break;
      default:
        require(false);
        break;
    }
    d.ind[i] = 0;
  } else if (optype == Op::DEL) {
    ;
  } else if (i == getcol("seq").no) {
    d.seq = g_seq++;
    d.ind[i] = 0;
  } else if (optype == Op::INS && !g_opts.no_implicit_nulls && c.nullable &&
             urandom(10, 100)) {
    d.noop |= (1 << i);
    d.ind[i] = 1;  // implicit NULL value is known
  } else if (optype == Op::UPD && !g_opts.no_missing_update &&
             urandom(10, 100)) {
    d.noop |= (1 << i);
    d.ind[i] = -1;  // fixed up in caller
  } else if (!g_opts.no_nulls && c.nullable && urandom(10, 100)) {
    d.ind[i] = 1;
  } else {
    switch (c.type) {
      case NdbDictionary::Column::Unsigned: {
        Uint32 *p = d.ptr[i].u32;
        uint u = urandom();
        *p = u;
      } break;
      case NdbDictionary::Column::Char: {
        char *p = d.ptr[i].ch;
        uint u = urandom(g_charlen);
        if (u == 0) u = urandom(g_charlen);  // 2x bias for non-empty
        uint j;
        for (j = 0; j < g_charlen; j++) {
          uint v = urandom((uint)strlen(g_charval));
          p[j] = j < u ? g_charval[v] : 0x20;
        }
      } break;
      case NdbDictionary::Column::Text:
      case NdbDictionary::Column::Blob: {
        const bool tinyblob = (c.type == NdbDictionary::Column::Blob);
        Data::Txt &txt = *d.ptr[i].txt;
        delete[] txt.val;
        txt.val = 0;
        if (g_opts.tweak & 1) {
          uint u = g_blobinlinesize + (tinyblob ? 0 : g_blobpartsize);
          uint v = (g_opts.tweak & 2) ? 0 : urandom((uint)strlen(g_charval));
          txt.val = new char[u];
          txt.len = u;
          memset(txt.val, g_charval[v], u);
          break;
        }
        uint u = urandom(tinyblob ? g_blobinlinesize : g_maxblobsize);
        u = urandom(u);  // 4x bias for smaller blobs
        u = urandom(u);
        txt.val = new char[u];
        txt.len = u;
        uint j = 0;
        while (j < u) {
          require(u > 0);
          uint k = 1 + urandom(u - 1);
          if (k > u - j) k = u - j;
          uint v = urandom((uint)strlen(g_charval));
          memset(&txt.val[j], g_charval[v], k);
          j += k;
        }
      } break;
      default:
        require(false);
        break;
    }
    d.ind[i] = 0;
  }
}

static void makeop(const Op *prev_op, Op *op, Uint32 pk1, Op::Type optype) {
  op->type = optype;
  const Data &dp = prev_op->data[0];
  Data &d0 = op->data[0];
  Data &d1 = op->data[1];
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = getcol(i);
    makedata(c, d0, pk1, optype);
    if (optype == Op::INS) {
      d1.ind[i] = -1;
    } else if (optype == Op::DEL) {
      require(dp.ind[i] >= 0);
      copycol(c, dp, d1);
    } else if (optype == Op::UPD) {
      require(dp.ind[i] >= 0);
      if (d0.ind[i] == -1)   // not updating this col
        copycol(c, dp, d0);  // must keep track of data
      copycol(c, dp, d1);
    } else {
      require(false);
    }
  }
  Uint32 pk1_tmp = ~(Uint32)0;
  reqrc(checkop(op, pk1_tmp) == 0);
  reqrc(pk1 == pk1_tmp);
}

static uint approxblobops(Op *op) {
  uint avg_blob_size = g_maxblobsize / 4;  // see makedata()
  uint avg_blob_ops = avg_blob_size / 2000;
  uint n = 0;
  if (!g_opts.no_blobs) {
    n += avg_blob_ops;
    if (!g_opts.one_blob) n += avg_blob_ops;
    if (op->type == Op::UPD) n *= 2;
  }
  return n;
}

static void makeops(Run &r) {
  ll1("makeops: " << r.tabname);
  Uint32 pk1 = 0;
  while (1) {
    if (g_opts.opstring == 0) {
      if (r.tableops + r.blobops >= g_opts.maxops)  // use up ops
        break;
      pk1 = urandom(g_opts.maxpk);
    } else {
      if (pk1 >= g_opts.maxpk)  // use up pks
        break;
    }
    ll2("makeops: pk1=" << pk1);
    // total op on the pk so far
    // optype either NUL=initial/deleted or INS=created
    Op *tot_op = r.pk_op[pk1];
    if (tot_op == 0) tot_op = r.pk_op[pk1] = getop(Op::OP, Op::NUL);
    require(tot_op->type == Op::NUL || tot_op->type == Op::INS);
    // add new commit chain to end
    Op *last_gci = tot_op;
    while (last_gci->next_gci != 0) last_gci = last_gci->next_gci;
    Op *gci_op = getop(Op::OP, Op::NUL);
    last_gci->next_gci = gci_op;
    Op *com_op = getop(Op::OP, Op::NUL);
    gci_op->next_com = com_op;
    // length of random chain
    uint len = ~0;
    if (g_opts.opstring == 0) {
      len = 1 + urandom(g_maxcom - 1);
      len = 1 + urandom(len - 1);  // 2x bias for short chain
    }
    uint n = 0;
    while (1) {
      // random or from current g_opts.opstring part
      Op::Type optype;
      if (g_opts.opstring == 0) {
        if (n == len) break;
        do {
          optype = (Op::Type)urandom(g_optypes);
        } while ((tot_op->type == Op::NUL &&
                  (optype == Op::DEL || optype == Op::UPD)) ||
                 (tot_op->type == Op::INS && optype == Op::INS));
      } else {
        const char *str = g_opstringpart[g_loop % g_opstringparts];
        uint m = (uint)strlen(str);
        uint k = tot_op->num_com + tot_op->num_op;
        require(k < m);
        char c = str[k];
        if (c == 'c') {
          if (k + 1 == m) pk1 += 1;
          break;
        }
        const char *p = "idu";
        const char *q = strchr(p, c);
        require(q != 0);
        optype = (Op::Type)(q - p);
      }
      Op *op = getop(Op::OP);
      makeop(tot_op, op, pk1, optype);
      r.tableops++;
      r.blobops += approxblobops(op);
      // add to end
      Op *last_op = com_op;
      while (last_op->next_op != 0) last_op = last_op->next_op;
      last_op->next_op = op;
      // merge into chain head and total op
      reqrc(compop(com_op, op, com_op) == 0);
      reqrc(compop(tot_op, op, tot_op) == 0);
      require(tot_op->type == Op::NUL || tot_op->type == Op::INS);
      // counts
      com_op->num_op += 1;
      tot_op->num_op += 1;
      n++;
    }
    // copy to gci level
    copyop(com_op, gci_op);
    tot_op->num_com += 1;
    r.gciops += 1;
    g_gciops += 1;
  }
  ll1("makeops: " << r.tabname << ": com recs = " << r.gciops);
}

static void selecttables() {
  uint i;
  for (i = 0; i < maxrun(); i++) run(i).skip = false;
  if (g_opts.opstring != 0) {
    ll1("using all tables due to fixed ops");
    return;
  }
  for (i = 0; i + 1 < maxrun(); i++) run(urandom(maxrun())).skip = true;
  uint cnt = 0;
  for (i = 0; i < maxrun(); i++) {
    if (!run(i).skip) {
      ll2("use table " << run(i).tabname);
      cnt++;
    }
  }
  ll0("selecttables: use " << cnt << "/" << maxrun() << " in this loop");
}

static void makeops() {
  selecttables();
  for (uint i = 0; i < maxrun(); i++)
    if (!run(i).skip) makeops(run(i));
  ll0("makeops: used records = " << g_usedops);
}

static int addndbop(Run &r, Op *op) {
  chkdb((g_op = g_con->getNdbOperation(r.tabname)) != 0);
  switch (op->type) {
    case Op::INS:
      chkdb(g_op->insertTuple() == 0);
      break;
    case Op::DEL:
      chkdb(g_op->deleteTuple() == 0);
      break;
    case Op::UPD:
      chkdb(g_op->updateTuple() == 0);
      break;
    default:
      require(false);
      break;
  }
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = getcol(i);
    const Data &d = op->data[0];
    if (!c.pk) continue;
    chkdb(g_op->equal(c.name, (const char *)d.ptr[i].v) == 0);
  }
  if (op->type != Op::DEL) {
    for (i = 0; i < ncol(); i++) {
      const Col &c = getcol(i);
      const Data &d = op->data[0];
      if (c.pk) continue;
      if (d.noop & (1 << i)) continue;
      require(d.ind[i] >= 0);
      if (!c.isblob()) {
        if (d.ind[i] == 0)
          chkdb(g_op->setValue(c.name, (const char *)d.ptr[i].v) == 0);
        else
          chkdb(g_op->setValue(c.name, (const char *)0) == 0);
      } else {
        const Data::Txt &txt = *d.ptr[i].txt;
        g_bh = g_op->getBlobHandle(c.name);
        if (d.ind[i] == 0)
          chkdb(g_bh->setValue(txt.val, txt.len) == 0);
        else
          chkdb(g_bh->setValue(0, 0) == 0);
        g_bh = 0;
      }
    }
  }
  g_op = 0;
  return 0;
}

static int runops() {
  ll1("runops");
  Op *gci_op[g_maxtab][g_maxpk];
  uint left = 0;  // number of table pks with ops
  Uint32 pk1;
  int i;
  for (i = 0; i < (int)maxrun(); i++) {
    Run &r = run(i);
    for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
      gci_op[i][pk1] = 0;
      // total op on the pk
      Op *tot_op = r.pk_op[pk1];
      if (tot_op == 0) continue;
      if (tot_op->next_gci == 0) {
        require(g_loop != 0 && tot_op->type == Op::INS);
        continue;
      }
      // first commit chain
      require(tot_op->next_gci != 0);
      gci_op[i][pk1] = tot_op->next_gci;
      left++;
    }
  }

  while (left != 0) {
    unsigned int i = urandom(maxrun());
    pk1 = urandom(g_opts.maxpk);
    if (gci_op[i][pk1] == 0) continue;
    Run &r = run(i);
    // do the ops in one transaction
    chkdb((g_con = g_ndb->startTransaction()) != 0);
    Op *com_op = gci_op[i][pk1]->next_com;
    require(com_op != 0);
    // first op in chain
    Op *op = com_op->next_op;
    require(op != 0);
    while (op != 0) {
      ll2("runops:" << *op);
      chkrc(addndbop(r, op) == 0);
      op = op->next_op;
    }
    chkdb(g_con->execute(Commit) == 0);
    Uint64 val;
    g_con->getGCI(&val);
    gci_op[i][pk1]->gci = com_op->gci = val;
    ll2("commit: " << run(i).tabname << " gci=" << com_op->gci);
    g_ndb->closeTransaction(g_con);
    g_con = 0;
    // next chain
    gci_op[i][pk1] = gci_op[i][pk1]->next_gci;
    if (gci_op[i][pk1] == 0) {
      require(left != 0);
      left--;
    }
  }
  require(left == 0);
  return 0;
}

// move com chains with same gci under same gci entry
static void mergeops(Run &r) {
  ll2("mergeops: " << r.tabname);
  uint mergecnt = 0;
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    Op *tot_op = r.pk_op[pk1];
    if (tot_op == 0) continue;
    Op *gci_op = tot_op->next_gci;
    if (gci_op == 0) {
      require(g_loop != 0 && tot_op->type == Op::INS);
      continue;
    }
    while (gci_op != 0) {
      Op *com_op = gci_op->next_com;
      require(com_op != 0);
      require(com_op->next_com == 0);
      require(gci_op->gci == com_op->gci);
      Op *last_com = com_op;
      Op *gci_op2 = gci_op->next_gci;
      while (gci_op2 != 0 && gci_op->gci == gci_op2->gci) {
        // move link to com level
        last_com = last_com->next_com = gci_op2->next_com;
        // merge to gci
        reqrc(compop(gci_op, gci_op2, gci_op) == 0);
        // move to next and discard
        Op *tmp_op = gci_op2;
        gci_op2 = gci_op2->next_gci;
        freeop(tmp_op);
        mergecnt++;
        require(r.gciops != 0 && g_gciops != 0);
        r.gciops--;
        g_gciops--;
      }
      gci_op = gci_op->next_gci = gci_op2;
    }
  }
  ll1("mergeops: " << r.tabname << ": gci recs = " << r.gciops);
}

static void mergeops() {
  for (uint i = 0; i < maxrun(); i++) mergeops(run(i));
  ll1("mergeops: used recs = " << g_usedops << " gci recs = " << g_gciops);
}

// set bit for equal post/pre data in UPD, for use in event match
static void cmppostpre(Run &r) {
  ll2("cmppostpre: " << r.tabname);
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    Op *tot_op = r.pk_op[pk1];
    Op *gci_op = tot_op ? tot_op->next_gci : 0;
    while (gci_op != 0) {
      if (gci_op->type == Op::UPD) {
        Data(&d)[2] = gci_op->data;
        uint i;
        for (i = 0; i < ncol(); i++) {
          const Col &c = getcol(i);
          bool eq = (d[0].ind[i] == 1 && d[1].ind[i] == 1) ||
                    (d[0].ind[i] == 0 && d[1].ind[i] == 0 &&
                     cmpcol(c, d[0], d[1]) == 0);
          if (eq) {
            d[0].ppeq |= (1 << i);
            d[1].ppeq |= (1 << i);
          }
        }
      }
      gci_op = gci_op->next_gci;
    }
  }
}

static void cmppostpre() {
  ll1("cmppostpre");
  for (uint i = 0; i < maxrun(); i++) cmppostpre(run(i));
}

static int findevent(const NdbEventOperation *evt_op) {
  uint i;
  for (i = 0; i < maxrun(); i++) {
    if (run(i).evt_op == evt_op) break;
  }
  chkrc(i < maxrun());
  return i;
}

static void geteventdata(Run &r) {
  Data(&d)[2] = g_rec_ev->data;
  int i, j;
  for (j = 0; j < 2; j++) {
    for (i = 0; i < (int)ncol(); i++) {
      const Col &c = getcol(i);
      int ind, ret;
      if (!c.isblob()) {
        NdbRecAttr *ra = r.ev_ra[j][i];
        ind = ra->isNULL();
      } else {
        NdbBlob *bh = r.ev_bh[j][i];
        ret = bh->getDefined(ind);
        require(ret == 0);
        if (ind == 0) {  // value was returned and is not NULL
          Data::Txt &txt = *d[j].ptr[i].txt;
          Uint64 len64;
          ret = bh->getLength(len64);
          require(ret == 0);
          txt.len = (uint)len64;
          delete[] txt.val;
          txt.val = new char[txt.len];
          memset(txt.val, 'X', txt.len);
          Uint32 len = txt.len;
          ret = bh->readData(txt.val, len);
          require(ret == 0 && len == txt.len);
        }
      }
      d[j].ind[i] = ind;
    }
  }
}

static int addgcievents(Uint64 gci) {
  ll1("getgcieventops");
  uint count = 0;
  uint seen_current = 0;
  Uint32 iter = 0;
  while (1) {
    Uint32 evtypes = 0;
    const NdbEventOperation *evt_op =
        g_ndb->getGCIEventOperations(&iter, &evtypes);
    if (evt_op == 0) break;
    // evt_op->getGCI() is not defined yet
    int i;
    chkrc((i = findevent(evt_op)) != -1);
    run(i).addevtypes(gci, evtypes, 0);
    seen_current += (g_evt_op == evt_op);
    count++;
  }
  chkrc(seen_current == 1);
  ll1("addgcievents: " << count);
  return 0;
}

static int runevents() {
  ll1("runevents");
  uint mspoll = 1000;
  uint npoll = 6;  // strangely long delay
  ll1("poll " << npoll);
  Uint64 gci = (Uint64)0;
  while (npoll != 0) {
    npoll--;
    int ret;
    ret = g_ndb->pollEvents(mspoll);
    if (ret <= 0) continue;
    while (1) {
      g_rec_ev->init(Op::EV);
      g_evt_op = g_ndb->nextEvent();
      if (g_evt_op == 0) break;
      Uint64 newgci = g_evt_op->getGCI();
      require(newgci != 0);
      g_rec_ev->gci = newgci;
      if (gci != newgci) {
        ll1("new gci: " << gci << " -> " << newgci);
        gci = newgci;
        // add slot in each tab|e
        uint i;
        for (i = 0; i < maxtab(); i++) chkrc(run(i).addgci(gci) == 0);
        chkrc(addgcievents(gci) == 0);
      }
      int i;
      chkrc((i = findevent(g_evt_op)) != -1);
      Run &r = run(i);
      NdbDictionary::Event::TableEvent evtype = g_evt_op->getEventType();
      chkrc(seteventtype(g_rec_ev, evtype) == 0);
      r.addevtypes(gci, (Uint32)evtype, 1);
      geteventdata(r);
      ll2("runevents: EVT: " << *g_rec_ev);
      // check basic sanity
      Uint32 pk1 = ~(Uint32)0;
      chkrc(checkop(g_rec_ev, pk1) == 0);
      // add to events
      Op *tot_ev = r.pk_ev[pk1];
      if (tot_ev == 0) tot_ev = r.pk_ev[pk1] = getop(Op::EV);
      Op *last_ev = tot_ev;
      while (last_ev->next_ev != 0) last_ev = last_ev->next_ev;
      // copy and add
      Op *ev = getop(Op::EV);
      copyop(g_rec_ev, ev);
      g_rec_ev->freemem();
      last_ev->next_ev = ev;
      g_num_ev++;
    }
  }
  ll1("runevents: used ops = " << g_usedops << " events = " << g_num_ev);
  return 0;
}

static int cmpopevdata(const Data &d1, const Data &d2) {
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col &c = getcol(i);
    if (cmpcol(c, d1, d2) != 0) {
      if ((d1.ppeq & (1 << i)) && d2.ind[i] == -1)
        ;  // post/pre data equal and no event data returned is OK
      else
        return 1;
    }
  }
  return 0;
}

// compare operation to event data
static int cmpopevdata(const Data (&d1)[2], const Data (&d2)[2]) {
  if (cmpopevdata(d1[0], d2[0]) != 0) return 1;
  if (cmpopevdata(d1[1], d2[1]) != 0) return 1;
  return 0;
}

static int matchevent(Run &r, Op *ev) {
  Data(&d2)[2] = ev->data;
  // get PK
  Uint32 pk1 = d2[0].pk1;
  chkrc(pk1 < g_opts.maxpk);
  // on error repeat and print details
  uint loop = 0;
  while (loop <= 1) {
    int g_loglevel = loop == 0 ? g_opts.loglevel : 2;
    ll1("matchevent: " << r.tabname << ": pk1=" << pk1 << " type=" << ev->type);
    ll2("EVT: " << *ev);
    Op *tot_op = r.pk_op[pk1];
    Op *gci_op = tot_op ? tot_op->next_gci : 0;
    uint pos = 0;
    bool ok = false;
    while (gci_op != 0) {
      ll2("GCI: " << *gci_op);
      // print details
      Op *com_op = gci_op->next_com;
      require(com_op != 0);
      while (com_op != 0) {
        ll2("COM: " << *com_op);
        Op *op = com_op->next_op;
        require(op != 0);
        while (op != 0) {
          ll2("OP : " << *op);
          op = op->next_op;
        }
        com_op = com_op->next_com;
      }
      // match against GCI op
      if (gci_op->type != Op::NUL) {
        const Data(&d1)[2] = gci_op->data;
        if (cmpopevdata(d1, d2) == 0) {
          bool tmpok = true;
          if (gci_op->type != ev->type) {
            ll2("***: wrong type " << gci_op->type << " != " << ev->type);
            tmpok = false;
          }
          if (gci_op->match) {
            ll2("***: duplicate match");
            tmpok = false;
          }
          if (pos != r.ev_pos[pk1]) {
            ll2("***: wrong pos " << pos << " != " << r.ev_pos[pk1]);
            tmpok = false;
          }
          if (gci_op->gci != ev->gci) {
            ll2("***: wrong gci " << gci_op->gci << " != " << ev->gci);
            tmpok = false;
          }
          if (tmpok) {
            ok = gci_op->match = true;
            ll2("match");
          }
        }
        pos++;
      }
      gci_op = gci_op->next_gci;
    }
    if (ok) {
      ll2("matchevent: match");
      return 0;
    }
    ll0("matchevent: ERROR: no match");
    if (g_loglevel >= 2) return -1;
    loop++;
  }
  return 0;
}

static int matchevents(Run &r) {
  ll1("matchevents: " << r.tabname);
  uint nomatch = 0;
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    Op *tot_ev = r.pk_ev[pk1];
    if (tot_ev == 0) continue;
    Op *ev = tot_ev->next_ev;
    while (ev != 0) {
      if (matchevent(r, ev) < 0) nomatch++;
      r.ev_pos[pk1]++;
      ev = ev->next_ev;
    }
  }
  chkrc(nomatch == 0);
  return 0;
}

static int matchevents() {
  ll1("matchevents");
  for (uint i = 0; i < maxrun(); i++) chkrc(matchevents(run(i)) == 0);
  return 0;
}

static int matchops(Run &r) {
  ll1("matchops: " << r.tabname);
  uint nomatch = 0;
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    Op *tot_op = r.pk_op[pk1];
    if (tot_op == 0) continue;
    Op *gci_op = tot_op->next_gci;
    while (gci_op != 0) {
      if (gci_op->type == Op::NUL) {
        ll2("GCI: " << *gci_op << " [skip NUL]");
      } else if (gci_op->match) {
        ll2("GCI: " << *gci_op << " [match OK]");
      } else {
        ll0("GCI: " << *gci_op);
        Op *com_op = gci_op->next_com;
        require(com_op != 0);
        ll0("COM: " << *com_op);
        Op *op = com_op->next_op;
        require(op != 0);
        while (op != 0) {
          ll0("OP : " << *op);
          op = op->next_op;
        }
        ll0("no matching event");
        nomatch++;
      }
      gci_op = gci_op->next_gci;
    }
  }
  chkrc(nomatch == 0);
  return 0;
}

static int matchops() {
  ll1("matchops");
  for (uint i = 0; i < maxrun(); i++) chkrc(matchops(run(i)) == 0);
  return 0;
}

static int matchgcievents(Run &r) {
  ll1("matchgcievents: " << r.tabname);
  uint i;
  for (i = 0; i < r.gcicnt; i++) {
    Uint32 t0 = r.gcievtypes[i][0];
    Uint32 t1 = r.gcievtypes[i][1];
    ll1("gci: " << r.gcinum[i] << hex << " report: " << t0 << " seen: " << t1);

    if (r.skip) chkrc(t0 == 0 && t1 == 0);
    if (t0 == 0 && t1 == 0) continue;

    // check if not reported event op seen
    chkrc(t0 != 0);
    // check if not reported event type seen
    chkrc((~t0 & t1) == 0);

    // the other way does not work under merge
    if (g_opts.separate_events) {
      // check if reported event op not seen
      chkrc(t1 != 0);
      // check if reported event type not seen
      chkrc((t0 & ~t1) == 0);
    }
  }
  return 0;
}

static int matchgcievents() {
  ll1("matchgcievents");
  for (uint i = 0; i < maxrun(); i++) chkrc(matchgcievents(run(i)) == 0);
  return 0;
}

static void setseed(int n) {
  uint seed;
  if (n == -1) {
    if (g_opts.seed == 0) return;
    if (g_opts.seed != (uint)-1)
      seed = (uint)g_opts.seed;
    else
      seed = 1 + NdbHost_GetProcessId();
  } else {
    if (g_opts.seed != 0) return;
    seed = n;
  }
  ll0("seed=" << seed);
  ndb_srand(seed);
}

static int runtest() {
  setseed(-1);
  initrun();
  chkrc(createtables() == 0);
  chkrc(createevents() == 0);
  for (g_loop = 0; g_opts.loop == 0 || g_loop < g_opts.loop; g_loop++) {
    ll0("=== loop " << g_loop << " ===");
    setseed(g_loop);
    resetmem();
    chkrc(scantable() == 0);  // alternative: save tot_op for loop > 0
    makeops();
    g_rec_ev = getop(Op::EV);
    chkrc(createeventop() == 0);
    chkrc(executeeventop() == 0);
    chkrc(waitgci(3) == 0);
    chkrc(runops() == 0);
    if (!g_opts.separate_events) mergeops();
    cmppostpre();
    chkrc(runevents() == 0);
    ll0("counts: gci ops = " << g_gciops << " ev ops = " << g_num_ev);
    chkrc(matchevents() == 0);
    chkrc(matchops() == 0);
    chkrc(matchgcievents() == 0);
    chkrc(dropeventops() == 0);
    // time erases everything..
    chkrc(waitgci(1) == 0);
  }
  chkrc(dropevents() == 0);
  chkrc(droptables() == 0);
  resetmem();
  deleteops();
  return 0;
}

static struct my_option my_long_options[] = {
    NdbStdOpt::usage,
    NdbStdOpt::help,
    NdbStdOpt::version,
    NdbStdOpt::ndb_connectstring,
    NdbStdOpt::mgmd_host,
    NdbStdOpt::connectstring,
    NdbStdOpt::ndb_nodeid,
    NdbStdOpt::connect_retry_delay,
    NdbStdOpt::connect_retries,
    NDB_STD_OPT_DEBUG{"abort-on-error", NDB_OPT_NOSHORT,
                      "Do abort() on any error", &g_opts.abort_on_error,
                      &g_opts.abort_on_error, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
                      0, 0},
    {"loglevel", NDB_OPT_NOSHORT,
     "Logging level in this program 0-3 (default 0)", &g_opts.loglevel,
     &g_opts.loglevel, 0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"loop", NDB_OPT_NOSHORT, "Number of test loops (default 5, 0=forever)",
     &g_opts.loop, &g_opts.loop, 0, GET_INT, REQUIRED_ARG, 5, 0, 0, 0, 0, 0},
    {"maxops", NDB_OPT_NOSHORT,
     "Approx number of PK operations per table (default 1000)", &g_opts.maxops,
     &g_opts.maxops, 0, GET_UINT, REQUIRED_ARG, 1000, 0, 0, 0, 0, 0},
    {"maxpk", NDB_OPT_NOSHORT,
     "Number of different PK values (default 10, max 1000)", &g_opts.maxpk,
     &g_opts.maxpk, 0, GET_UINT, REQUIRED_ARG, 10, 0, 0, 0, 0, 0},
    {"maxtab", NDB_OPT_NOSHORT, "Number of tables (default 10, max 100)",
     &g_opts.maxtab, &g_opts.maxtab, 0, GET_INT, REQUIRED_ARG, 10, 0, 0, 0, 0,
     0},
    {"no-blobs", NDB_OPT_NOSHORT, "Omit blob attributes (5.0: true)",
     &g_opts.no_blobs, &g_opts.no_blobs, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"no-implicit-nulls", NDB_OPT_NOSHORT,
     "Insert must include all attrs"
     " i.e. no implicit NULLs",
     &g_opts.no_implicit_nulls, &g_opts.no_implicit_nulls, 0, GET_BOOL, NO_ARG,
     0, 0, 0, 0, 0, 0},
    {"no-missing-update", NDB_OPT_NOSHORT,
     "Update must include all non-PK attrs", &g_opts.no_missing_update,
     &g_opts.no_missing_update, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"no-multiops", NDB_OPT_NOSHORT, "Allow only 1 operation per commit",
     &g_opts.no_multiops, &g_opts.no_multiops, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
     0, 0},
    {"no-nulls", NDB_OPT_NOSHORT, "Create no NULL values", &g_opts.no_nulls,
     &g_opts.no_nulls, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"one-blob", NDB_OPT_NOSHORT, "Only one blob attribute (default 2)",
     &g_opts.one_blob, &g_opts.one_blob, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"opstring", NDB_OPT_NOSHORT,
     "Operations to run e.g. idiucdc (c is commit) or"
     " iuuc:uudc (the : separates loops)",
     &g_opts.opstring, &g_opts.opstring, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0,
     0, 0, 0, 0},
    {"seed", NDB_OPT_NOSHORT, "Random seed (0=loop number, default -1=random)",
     &g_opts.seed, &g_opts.seed, 0, GET_INT, REQUIRED_ARG, -1, 0, 0, 0, 0, 0},
    {"separate-events", NDB_OPT_NOSHORT,
     "Do not combine events per GCI (5.0: true)", &g_opts.separate_events,
     &g_opts.separate_events, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"tweak", NDB_OPT_NOSHORT, "Whatever the source says", &g_opts.tweak,
     &g_opts.tweak, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"use-table", NDB_OPT_NOSHORT, "Use existing tables", &g_opts.use_table,
     &g_opts.use_table, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"blob-version", NDB_OPT_NOSHORT, "Blob version 1 or 2 (default 2)",
     &g_opts.blob_version, &g_opts.blob_version, 0, GET_INT, REQUIRED_ARG, 2, 0,
     0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

static int checkopts() {
  if (g_opts.separate_events) {
    g_opts.no_blobs = true;
  }
  if (g_opts.no_multiops) {
    g_maxcom = 1;
  }
  if (g_opts.opstring != 0) {
    uint len = (uint)strlen(g_opts.opstring);
    char *str = new char[len + 1];
    memcpy(str, g_opts.opstring, len + 1);
    char *s = str;
    while (1) {
      g_opstringpart[g_opstringparts++] = s;
      s = strchr(s, ':');
      if (s == 0) break;
      *s++ = 0;
    }
    uint i;
    for (i = 0; i < g_opstringparts; i++) {
      const char *s = g_opstringpart[i];
      while (*s != 0) {
        if (strchr("iduc", *s++) == 0) {
          ll0("opstring chars are i,d,u,c");
          return -1;
        }
      }
      if (s == g_opstringpart[i] || s[-1] != 'c') {
        ll0("opstring chain must end in 'c'");
        return -1;
      }
    }
  }
  if (g_opts.no_nulls) {
    g_opts.no_implicit_nulls = true;
  }
  if (g_opts.maxpk > g_maxpk || g_opts.maxtab > (int)g_maxtab) {
    return -1;
  }
  if (g_opts.blob_version < 1 || g_opts.blob_version > 2) {
    return -1;
  }
  return 0;
}

static int doconnect() {
  g_ncc = new Ndb_cluster_connection();
  g_ncc->configure_tls(opt_tls_search_path, opt_mgm_tls);
  chkdb(g_ncc->connect(30) == 0);
  g_ndb = new Ndb(g_ncc, "TEST_DB");
  chkdb(g_ndb->init() == 0 && g_ndb->waitUntilReady(30) == 0);
  return 0;
}

int main(int argc, char **argv) {
  ndb_init();
  const char *progname =
      strchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
  ndbout << progname;
  for (int i = 1; i < argc; i++) ndbout << " " << argv[i];
  ndbout << endl;
  int ret;
  ret = handle_options(&argc, &argv, my_long_options, ndb_std_get_one_option);
  if (ret != 0 || argc != 0 || checkopts() != 0)
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  if (doconnect() == 0 && runtest() == 0) {
    delete g_ndb;
    delete g_ncc;
    return NDBT_ProgramExit(NDBT_OK);
  }
  dropeventops(true);
  dropevents(true);
  delete g_ndb;
  delete g_ncc;
  return NDBT_ProgramExit(NDBT_FAILED);
}
