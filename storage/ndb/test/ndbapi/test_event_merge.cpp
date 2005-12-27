/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NdbApi.hpp>
#include <NdbTest.hpp>
#include <my_sys.h>
#include <ndb_version.h>

#if NDB_VERSION_D < MAKE_VERSION(5, 1, 0)
#define version50
#else
#undef version50
#endif

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
 * by default merged.  This is required to read blob data via NdbBlob.
 *
 * This test program ignores Blob columns in version 5.0.
 *
 * There are 5 ways (ignoring NUL operand) to compose 2 ops:
 *                      5.0 bugs        5.1 bugs
 * INS o DEL = NUL
 * INS o UPD = INS                       5.1
 * DEL o INS = UPD      type=INS         5.1
 * UPD o DEL = DEL      no event
 * UPD o UPD = UPD
 */

struct Opts {
  my_bool abort_on_error;
  int loglevel;
  uint loop;
  uint maxops;
  uint maxpk;
  const char* opstr;
  uint seed;
  my_bool separate_events;
  my_bool use_table;
};

static Opts g_opts;
static const uint g_maxops = 10000;
static const uint g_maxpk = 100;

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static NdbDictionary::Dictionary* g_dic = 0;
static NdbTransaction* g_con = 0;
static NdbOperation* g_op = 0;

static const char* g_tabname = "tem1";
static const char* g_evtname = "tem1ev1";
static const uint g_charlen = 5;
static const char* g_csname = "latin1_swedish_ci";

static const NdbDictionary::Table* g_tab = 0;
static const NdbDictionary::Event* g_evt = 0;

static NdbEventOperation* g_evt_op = 0;

static uint
urandom(uint n)
{
  uint r = (uint)random();
  if (n != 0)
    r = r % n;
  return r;
}

static int& g_loglevel = g_opts.loglevel; // default log level

#define chkdb(x) \
  do { if (x) break; ndbout << "line " << __LINE__ << " FAIL " << #x << endl; errdb(); if (g_opts.abort_on_error) abort(); return -1; } while (0)

#define chkrc(x) \
  do { if (x) break; ndbout << "line " << __LINE__ << " FAIL " << #x << endl; if (g_opts.abort_on_error) abort(); return -1; } while (0)

#define reqrc(x) \
  do { if (x) break; ndbout << "line " << __LINE__ << " ASSERT " << #x << endl; abort(); } while (0)

#define ll0(x) \
  do { if (g_loglevel < 0) break; ndbout << x << endl; } while (0)

#define ll1(x) \
  do { if (g_loglevel < 1) break; ndbout << x << endl; } while (0)

#define ll2(x) \
  do { if (g_loglevel < 2) break; ndbout << x << endl; } while (0)

static void
errdb()
{
  uint any = 0;
  if (g_ndb != 0) {
    const NdbError& e = g_ndb->getNdbError();
    if (e.code != 0)
      ll0(++any << " ndb: error " << e);
  }
  if (g_dic != 0) {
    const NdbError& e = g_dic->getNdbError();
    if (e.code != 0)
      ll0(++any << " dic: error " << e);
  }
  if (g_con != 0) {
    const NdbError& e = g_con->getNdbError();
    if (e.code != 0)
      ll0(++any << " con: error " << e);
  }
  if (g_op != 0) {
    const NdbError& e = g_op->getNdbError();
    if (e.code != 0)
      ll0(++any << " op: error " << e);
  }
  if (g_evt_op != 0) {
    const NdbError& e = g_evt_op->getNdbError();
    if (e.code != 0)
      ll0(++any << " evt_op: error " << e);
  }
  if (! any)
    ll0("unknown db error");
}

struct Col {
  uint no;
  const char* name;
  NdbDictionary::Column::Type type;
  bool pk;
  bool nullable;
  uint length;
  uint size;
};

static Col g_col[] = {
  { 0, "pk1", NdbDictionary::Column::Unsigned, true, false, 1, 4 },
  { 1, "pk2", NdbDictionary::Column::Char, true, false,  g_charlen, g_charlen },
  { 2, "seq", NdbDictionary::Column::Unsigned,  false, false, 1, 4 },
  { 3, "cc1", NdbDictionary::Column::Char, false, true, g_charlen, g_charlen }
};

static const uint g_ncol = sizeof(g_col)/sizeof(g_col[0]);

static const Col&
getcol(uint i)
{
  if (i < g_ncol)
    return g_col[i];
  assert(false);
  return g_col[g_ncol];
}

static const Col&
getcol(const char* name)
{
  uint i;
  for (i = 0; i < g_ncol; i++)
    if (strcmp(g_col[i].name, name) == 0)
      break;
  return getcol(i);
}

static int
createtable()
{
  g_tab = 0;
  NdbDictionary::Table tab(g_tabname);
  tab.setLogging(false);
  CHARSET_INFO* cs;
  chkrc((cs = get_charset_by_name(g_csname, MYF(0))) != 0);
  uint i;
  for (i = 0; i < g_ncol; i++) {
    const Col& c = g_col[i];
    NdbDictionary::Column col(c.name);
    col.setType(c.type);
    col.setPrimaryKey(c.pk);
    if (! c.pk)
      col.setNullable(true);
    col.setLength(c.length);
    switch (c.type) {
    case NdbDictionary::Column::Unsigned:
      break;
    case NdbDictionary::Column::Char:
      col.setLength(c.length);
      col.setCharset(cs);
      break;
    default:
      assert(false);
      break;
    }
    tab.addColumn(col);
  }
  g_dic = g_ndb->getDictionary();
  if (! g_opts.use_table) {
    if (g_dic->getTable(g_tabname) != 0)
      chkdb(g_dic->dropTable(g_tabname) == 0);
    chkdb(g_dic->createTable(tab) == 0);
  }
  chkdb((g_tab = g_dic->getTable(g_tabname)) != 0);
  g_dic = 0;
  if (! g_opts.use_table) {
    // extra row for GCI probe
    chkdb((g_con = g_ndb->startTransaction()) != 0);
    chkdb((g_op = g_con->getNdbOperation(g_tabname)) != 0);
    chkdb(g_op->insertTuple() == 0);
    Uint32 pk1;
    char pk2[g_charlen];
    pk1 = g_maxpk;
    memset(pk2, 0x20, g_charlen);
    chkdb(g_op->equal("pk1", (char*)&pk1) == 0);
    chkdb(g_op->equal("pk2", (char*)&pk2[0]) == 0);
    chkdb(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);
    g_op = 0;
    g_con = 0;
  }
  return 0;
}

static int
droptable()
{
  if (! g_opts.use_table) {
    g_dic = g_ndb->getDictionary();
    chkdb(g_dic->dropTable(g_tab->getName()) == 0);
    g_tab = 0;
    g_dic = 0;
  }
  return 0;
}

struct Data {
  Uint32 pk1;
  char pk2[g_charlen];
  Uint32 seq;
  char cc1[g_charlen];
  void* ptr[g_ncol];
  int ind[g_ncol]; // -1 = no data, 1 = NULL, 0 = not NULL
  void init() {
    uint i;
    pk1 = 0;
    memset(pk2, 0, sizeof(pk2));
    seq = 0;
    memset(cc1, 0, sizeof(cc1));
    ptr[0] = &pk1;
    ptr[1] = pk2;
    ptr[2] = &seq;
    ptr[3] = cc1;
    for (i = 0; i < g_ncol; i++)
      ind[i] = -1;
  }
};

static NdbOut&
operator<<(NdbOut& out, const Data& d)
{
  uint i;
  for (i = 0; i < g_ncol; i++) {
    const Col& c = getcol(i);
    out << (i == 0 ? "" : " ") << c.name << "=";
    if (d.ind[i] == -1)
      continue;
    if (d.ind[i] == 1) {
      out << "NULL";
      continue;
    }
    switch (c.type) {
    case NdbDictionary::Column::Unsigned:
      out << *(Uint32*)d.ptr[i];
      break;
    case NdbDictionary::Column::Char:
      {
        char buf[g_charlen + 1];
        memcpy(buf, d.ptr[i], g_charlen);
        uint n = g_charlen;
        while (1) {
          buf[n] = 0;
          if (n == 0 || buf[n - 1] != 0x20)
            break;
          n--;
        }
        out << buf;
      }
      break;
    default:
      out << "?";
      break;
    }
  }
  return out;
}

static const uint g_optypes = 3; // real ops 0-2

/*
 * Represents single or composite operation or received event.  The
 * post/pre data is either computed here for operations or received from
 * the event.
 */
struct Op { // single or composite
  enum Kind { OP = 1, EV = 2 };
  enum Type { NUL = -1, INS, DEL, UPD };
  Kind kind;
  Type type;
  Op* next_op; // within one commit
  Op* next_com; // next commit chain or next event
  uint num_op;
  uint num_com;
  Data data[2]; // 0-post 1-pre
  bool match; // matched to event
  void init() {
    assert(kind == OP || kind == EV);
    type = NUL;
    next_op = next_com = 0;
    num_op = num_com = 0;
    data[0].init();
    data[1].init();
    match = false;
  }
};

static NdbOut&
operator<<(NdbOut& out, Op::Type t)
{
  switch (t) {
  case Op::NUL:
    out << "NUL";
    break;
  case Op::INS:
    out << "INS";
    break;
  case Op::DEL:
    out << "DEL";
    break;
  case Op::UPD:
    out << "UPD";
    break;
  default:
    out << (int)t;
    break;
  }
  return out;
}

static NdbOut&
operator<<(NdbOut& out, const Op& op)
{
  out << "t=" << op.type;
  out << " " << op.data[0];
  out << " [" << op.data[1] << "]";
  return out;
}

static int
seteventtype(Op* ev, NdbDictionary::Event::TableEvent te)
{
  Op::Type t = Op::NUL;
  switch (te) {
  case NdbDictionary::Event::TE_INSERT:
    t = Op::INS;
    break;
  case NdbDictionary::Event::TE_DELETE:
    t = Op::DEL;
    break;
  case NdbDictionary::Event::TE_UPDATE:
    t = Op::UPD;
    break;
  default:
    ll0("EVT: " << *ev << ": bad event type" << (int)te);
    return -1;
  }
  ev->type = t;
  return 0;
}

static uint g_usedops = 0;
static uint g_usedevs = 0;
static Op g_oplist[g_maxops];
static Op g_evlist[g_maxops];
static uint g_maxcom = 8; // max ops per commit

static Op* g_pk_op[g_maxpk];
static Op* g_pk_ev[g_maxpk];
static uint g_seq = 0;

static NdbRecAttr* g_ra[2][g_ncol]; // 0-post 1-pre
static Op* g_rec_ev;
static uint g_ev_cnt[g_maxpk];

static uint
getfreeops()
{
  assert(g_opts.maxops >= g_usedops);
  return g_opts.maxops - g_usedops;
}

static uint
getfreeevs()
{
  assert(g_opts.maxops >= g_usedevs);
  return g_opts.maxops - g_usedevs;
}

static Op*
getop()
{
  if (g_usedops < g_opts.maxops) {
    Op* op = &g_oplist[g_usedops++];
    op->kind = Op::OP;
    op->init();
    return op;
  }
  assert(false);
  return 0;
}

static Op*
getev()
{
  if (g_usedevs < g_opts.maxops) {
    Op* ev = &g_evlist[g_usedevs++];
    ev->kind = Op::EV;
    ev->init();
    return ev;
  }
  assert(false);
  return 0;
}

static void
resetmem()
{
  int i, j;
  for (j = 0; j < 2; j++)
    for (i = 0; i < g_ncol; i++)
      g_ra[j][i] = 0;
  g_rec_ev = 0;
  for (i = 0; i < g_opts.maxpk; i++)
    g_pk_op[i] = 0;
  for (i = 0; i < g_opts.maxpk; i++)
    g_ev_cnt[i] = 0;
  g_seq = 0;
  g_usedops = 0;
  g_usedevs = 0;
}

struct Comp {
  Op::Type t1, t2, t3;
};

static Comp
g_comp[] = {
  { Op::INS, Op::DEL, Op::NUL },
  { Op::INS, Op::UPD, Op::INS },
  { Op::DEL, Op::INS, Op::UPD },
  { Op::UPD, Op::DEL, Op::DEL },
  { Op::UPD, Op::UPD, Op::UPD }
};

static const uint g_ncomp = sizeof(g_comp)/sizeof(g_comp[0]);

static int
checkop(const Op* op, Uint32& pk1)
{
  const Data (&d)[2] = op->data;
  Op::Type t = op->type;
  chkrc(t == Op::NUL || t == Op::INS || t == Op::DEL || t == Op::UPD);
  { const Col& c = getcol("pk1");
    chkrc(d[0].ind[c.no] == 0);
    pk1 = d[0].pk1;
    chkrc(pk1 < g_opts.maxpk);
  }
  uint i;
  for (i = 0; i < g_ncol; i++) {
    const Col& c = getcol(i);
    if (t != Op::NUL) {
      if (c.pk) {
        chkrc(d[0].ind[i] == 0); // even DEL has PK in post data
        if (t == Op::INS) {
          chkrc(d[1].ind[i] == -1);
        } else if (t == Op::DEL) {
#ifdef ndb_event_cares_about_pk_pre_data
          chkrc(d[1].ind[i] == -1);
#endif
        } else {
#ifdef ndb_event_cares_about_pk_pre_data
          chkrc(d[1].ind[i] == 0);
#endif
        }
      } else {
        if (t == Op::INS) {
          chkrc(d[0].ind[i] >= 0);
          chkrc(d[1].ind[i] == -1);
        } else if (t == Op::DEL) {
          chkrc(d[0].ind[i] == -1);
          chkrc(d[1].ind[i] >= 0);
        } else if (op->kind == Op::OP) {
          chkrc(d[0].ind[i] >= 0);
          chkrc(d[1].ind[i] >= 0);
        }
      }
    }
  }
  return 0;
}

static Comp*
comptype(Op::Type t1, Op::Type t2) // only non-NUL
{
  uint i;
  for (i = 0; i < g_ncomp; i++)
    if (g_comp[i].t1 == t1 && g_comp[i].t2 == t2)
      return &g_comp[i];
  return 0;
}

static void
copycol(const Col& c, const Data& d1, Data& d3)
{
  if ((d3.ind[c.no] = d1.ind[c.no]) != -1)
    memmove(d3.ptr[c.no], d1.ptr[c.no], c.size);
}

static void
copykeys(const Data& d1, Data& d3)
{
  uint i;
  for (i = 0; i < g_ncol; i++) {
    const Col& c = g_col[i];
    if (c.pk)
      copycol(c, d1, d3);
  }
}

static void
copydata(const Data& d1, Data& d3)
{
  uint i;
  for (i = 0; i < g_ncol; i++) {
    const Col& c = g_col[i];
    copycol(c, d1, d3);
  }
}

static void
copyop(const Op* op1, Op* op3)
{
  op3->type = op1->type;
  copydata(op1->data[0], op3->data[0]);
  copydata(op1->data[1], op3->data[1]);
  Uint32 pk1_tmp;
  reqrc(checkop(op3, pk1_tmp) == 0);
}

// not needed for ops
static void
compdata(const Data& d1, const Data& d2, Data& d3) // d2 overrides d1
{
  uint i;
  for (i = 0; i < g_ncol; i++) {
    const Col& c = g_col[i];
    const Data* d = 0;
    if (d1.ind[i] == -1 && d2.ind[i] == -1)
      d3.ind[i] = -1;
    else if (d1.ind[i] == -1 && d2.ind[i] != -1)
      d = &d2;
    else if (d1.ind[i] != -1 && d2.ind[i] == -1)
      d = &d1;
    else
      d = &d2;
    if (d != 0)
      copycol(c, *d, d3);
  }
}

static int
compop(const Op* op1, const Op* op2, Op* op3) // op1 o op2 = op3
{
  Comp* comp;
  if (op2->type == Op::NUL) {
    copyop(op1, op3);
    return 0;
  }
  if (op1->type == Op::NUL) {
    copyop(op2, op3);
    return 0;
  }
  chkrc((comp = comptype(op1->type, op2->type)) != 0);
  op3->type = comp->t3;
  copykeys(op2->data[0], op3->data[0]);
  if (op3->type != Op::DEL)
    copydata(op2->data[0], op3->data[0]);
  if (op3->type != Op::INS)
    copydata(op1->data[1], op3->data[1]);
  Uint32 pk1_tmp;
  reqrc(checkop(op3, pk1_tmp) == 0);
  // not eliminating identical post-pre fields
  return 0;
}

static int
createevent()
{
  ll1("createevent");
  g_evt = 0;
  g_dic = g_ndb->getDictionary();
  NdbDictionary::Event evt(g_evtname);
  evt.setTable(*g_tab);
  evt.addTableEvent(NdbDictionary::Event::TE_ALL);
  // pk always
  evt.addEventColumn("pk1");
  evt.addEventColumn("pk2");
  // simple cols
  evt.addEventColumn("seq");
  evt.addEventColumn("cc1");
  if (g_dic->getEvent(evt.getName()) != 0)
    chkdb(g_dic->dropEvent(evt.getName()) == 0);
  chkdb(g_dic->createEvent(evt) == 0);
  chkdb((g_evt = g_dic->getEvent(evt.getName())) != 0);
  g_dic = 0;
  return 0;
}

static int
dropevent()
{
  ll1("dropevent");
  g_dic = g_ndb->getDictionary();
  chkdb(g_dic->dropEvent(g_evt->getName()) == 0);
  g_evt = 0;
  g_dic = 0;
  return 0;
}

static int
createeventop()
{
  ll1("createeventop");
#ifdef version50
  uint bsz = 10 * g_opts.maxops;
  chkdb((g_evt_op = g_ndb->createEventOperation(g_evt->getName(), bsz)) != 0);
#else
  chkdb((g_evt_op = g_ndb->createEventOperation(g_evt->getName())) != 0);
#endif
  uint i;
  for (i = 0; i < g_ncol; i++) {
    const Col& c = g_col[i];
    Data (&d)[2] = g_rec_ev->data;
    switch (c.type) {
    case NdbDictionary::Column::Unsigned:
    case NdbDictionary::Column::Char:
      chkdb((g_ra[0][i] = g_evt_op->getValue(c.name, (char*)d[0].ptr[i])) != 0);
      chkdb((g_ra[1][i] = g_evt_op->getPreValue(c.name, (char*)d[1].ptr[i])) != 0);
      break;
    default:
      assert(false);
      break;
    }
  }
  return 0;
}

static int
dropeventop()
{
  ll1("dropeventop");
  chkdb(g_ndb->dropEventOperation(g_evt_op) == 0);
  g_evt_op = 0;
  return 0;
}

static int
waitgci() // wait for event to be installed and for at least 1 GCI to pass
{
  const uint ngci = 3;
  ll1("waitgci " << ngci);
  Uint32 gci[2];
  uint i = 0;
  while (1) {
    chkdb((g_con = g_ndb->startTransaction()) != 0);
    { // forced to exec a dummy op
      Uint32 pk1;
      char pk2[g_charlen];
      pk1 = g_maxpk;
      memset(pk2, 0x20, g_charlen);
      chkdb((g_op = g_con->getNdbOperation(g_tabname)) != 0);
      chkdb(g_op->readTuple() == 0);
      chkdb(g_op->equal("pk1", (char*)&pk1) == 0);
      chkdb(g_op->equal("pk2", (char*)&pk2[0]) == 0);
      chkdb(g_con->execute(Commit) == 0);
      g_op = 0;
    }
    gci[i] = g_con->getGCI();
    g_ndb->closeTransaction(g_con);
    g_con = 0;
    if (i == 1 && gci[0] + ngci <= gci[1]) {
      ll1("waitgci: " << gci[0] << " " << gci[1]);
      break;
    }
    i = 1;
  }
  return 0;
}

static int
makeop(Op* op, Uint32 pk1, Op::Type t, const Op* prev_op)
{
  op->type = t;
  if (t != Op::INS)
    copydata(prev_op->data[0], op->data[1]);
  uint i;
  for (i = 0; i < g_ncol; i++) {
    const Col& c = getcol(i);
    Data (&d)[2] = op->data;
    if (i == getcol("pk1").no) {
      d[0].pk1 = pk1;
      d[0].ind[i] = 0;
      continue;
    }
    if (i == getcol("pk2").no) {
      sprintf(d[0].pk2, "%-*u", g_charlen, d[0].pk1);
      d[0].ind[i] = 0;
      continue;
    }
    if (t == Op::DEL) {
      d[0].ind[i] = -1;
      continue;
    }
    if (i == getcol("seq").no) {
      d[0].seq = g_seq++;
      d[0].ind[i] = 0;
      continue;
    }
    uint u;
    u = urandom(100);
    if (c.nullable && u < 20) {
      d[0].ind[i] = 1;
      continue;
    }
    switch (c.type) {
    case NdbDictionary::Column::Unsigned:
      {
        u = urandom(0);
        Uint32* p = (Uint32*)d[0].ptr[i];
        *p = u;
      }
      break;
    case NdbDictionary::Column::Char:
      {
        u = urandom(g_charlen);
        char* p = (char*)d[0].ptr[i];
        uint j;
        for (j = 0; j < g_charlen; j++) {
          uint v = urandom(3);
          p[j] = j < u ? "abcde"[v] : 0x20;
        }
      }
      break;
    default:
      assert(false);
      break;
    }
    d[0].ind[i] = 0;
  }
  Uint32 pk1_tmp = ~(Uint32)0;
  chkrc(checkop(op, pk1_tmp) == 0);
  reqrc(pk1 == pk1_tmp);
  return 0;
}

static void
makeop(Op* tot_op, Op* com_op, Uint32 pk1, Op::Type t)
{
  Op tmp_op;
  tmp_op.kind = Op::OP;
  Op* op = getop();
  reqrc(makeop(op, pk1, t, tot_op) == 0);
  // add to end
  Op* last_op = com_op;
  while (last_op->next_op != 0)
    last_op = last_op->next_op;
  last_op->next_op = op;
  // merge into chain head
  tmp_op.init();
  reqrc(compop(com_op, op, &tmp_op) == 0);
  copyop(&tmp_op, com_op);
  // merge into total op
  tmp_op.init();
  reqrc(compop(tot_op, op, &tmp_op) == 0);
  copyop(&tmp_op, tot_op);
  // counts
  com_op->num_op += 1;
  tot_op->num_op += 1;
}

static void
makeops()
{
  ll1("makeops");
  uint resv = g_opts.opstr == 0 ? 2 * g_opts.maxpk : 0; // for final deletes
  uint next = g_opts.opstr == 0 ? g_maxcom : strlen(g_opts.opstr);
  Op tmp_op;
  tmp_op.kind = Op::OP;
  Uint32 pk1 = 0;
  while (getfreeops() >= resv + 2 + next && pk1 < g_opts.maxpk) {
    if (g_opts.opstr == 0)
      pk1 = urandom(g_opts.maxpk);
    ll2("makeops: pk1=" << pk1 << " free=" << getfreeops());
    // total op on the pk so far
    // optype either NUL=initial/deleted or INS=created
    Op* tot_op = g_pk_op[pk1];
    if (tot_op == 0)
      tot_op = g_pk_op[pk1] = getop(); //1
    assert(tot_op->type == Op::NUL || tot_op->type == Op::INS);
    // add new commit chain to end
    Op* last_com = tot_op;
    while (last_com->next_com != 0)
      last_com = last_com->next_com;
    Op* com_op = getop(); //2
    last_com->next_com = com_op;
    // length of random chain
    uint len = ~0;
    if (g_opts.opstr == 0)
      len = 1 + urandom(g_maxcom - 1);
    ll2("makeops: com chain");
    uint n = 0;
    while (1) {
      // random or from g_opts.opstr
      Op::Type t;
      if (g_opts.opstr == 0) {
        if (n == len)
          break;
        do {
          t = (Op::Type)urandom(g_optypes);
        } while (tot_op->type == Op::NUL && (t == Op::DEL || t == Op::UPD) ||
                 tot_op->type == Op::INS && t == Op::INS);
      } else {
        uint m = strlen(g_opts.opstr);
        uint k = tot_op->num_com + tot_op->num_op;
        assert(k < m);
        char c = g_opts.opstr[k];
        if (c == 'c') {
          if (k + 1 == m)
            pk1 += 1;
          break;
        }
        const char* p = "idu";
        const char* q = strchr(p, c);
        assert(q != 0);
        t = (Op::Type)(q - p);
      }
      makeop(tot_op, com_op, pk1, t);
      assert(tot_op->type == Op::NUL || tot_op->type == Op::INS);
      n++;
    }
    tot_op->num_com += 1;
  }
  assert(getfreeops() >= resv);
  // terminate with DEL if necessary
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    Op* tot_op = g_pk_op[pk1];
    if (tot_op == 0)
      continue;
    if (tot_op->type == Op::NUL)
      continue;
    assert(g_opts.opstr == 0);
    Op* last_com = tot_op;
    while (last_com->next_com != 0)
      last_com = last_com->next_com;
    Op* com_op = getop(); //1
    last_com->next_com = com_op;
    makeop(tot_op, com_op, pk1, Op::DEL);
    assert(tot_op->type == Op::NUL);
    tot_op->num_com += 1;
  }
}

static int
addndbop(Op* op)
{
  chkdb((g_op = g_con->getNdbOperation(g_tabname)) != 0);
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
    assert(false);
    break;
  }
  uint i;
  for (i = 0; i < g_ncol; i++) {
    const Col& c = getcol(i);
    const Data& d = op->data[0];
    if (! c.pk)
      continue;
    chkdb(g_op->equal(c.name, (char*)d.ptr[i]) == 0);
  }
  if (op->type != Op::DEL) {
    for (i = 0; i < g_ncol; i++) {
      const Col& c = getcol(i);
      const Data& d = op->data[0];
      if (c.pk)
        continue;
      if (d.ind[i] == -1)
        continue;
      const char* ptr = d.ind[i] == 0 ? (char*)d.ptr[i] : 0;
      chkdb(g_op->setValue(c.name, ptr) == 0);
    }
  }
  g_op = 0;
  return 0;
}

static int
runops()
{
  ll1("runops");
  Uint32 pk1;
  const Op* com_op[g_maxpk];
  uint left = 0;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    com_op[pk1] = 0;
    // total op on the pk
    Op* tot_op = g_pk_op[pk1];
    if (tot_op == 0)
      continue;
    // first commit chain
    assert(tot_op->next_com != 0);
    com_op[pk1] = tot_op->next_com;
    left++;
  }
  while (left != 0) {
    pk1 = urandom(g_opts.maxpk);
    if (com_op[pk1] == 0)
      continue;
    // do the ops in one transaction
    ll2("runops: pk1=" << pk1);
    chkdb((g_con = g_ndb->startTransaction()) != 0);
    // first op in chain
    Op* op = com_op[pk1]->next_op;
    assert(op != 0);
    while (op != 0) {
      ll2("add op:" << *op);
      chkrc(addndbop(op) == 0);
      op = op->next_op;
    }
    chkdb(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);
    g_con = 0;
    // next chain
    com_op[pk1] = com_op[pk1]->next_com;
    if (com_op[pk1] == 0) {
      assert(left != 0);
      left--;
    }
  }
  assert(left == 0);
  return 0;
}

static int
matchevent(Op* ev)
{
  Op::Type t = ev->type;
  Data (&d)[2] = ev->data;
  // get PK
  Uint32 pk1 = d[0].pk1;
  chkrc(pk1 < g_opts.maxpk);
  // on error repeat and print details
  uint loop = 0;
  while (loop <= 1) {
    uint g_loglevel = loop == 0 ? g_opts.loglevel : 2;
    ll1("matchevent: pk1=" << pk1 << " type=" << t);
    ll2("EVT: " << *ev);
    Op* tot_op = g_pk_op[pk1];
    Op* com_op = tot_op ? tot_op->next_com : 0;
    uint cnt = 0;
    bool ok = false;
    while (com_op != 0) {
      ll2("COM: " << *com_op);
      Op* op = com_op->next_op;
      assert(op != 0);
      while (op != 0) {
        ll2("---: " << *op);
        op = op->next_op;
      }
      if (com_op->type != Op::NUL) {
        if (com_op->type == t) {
          const Data (&d2)[2] = com_op->data;
          if (t == Op::INS && d2[0].seq == d[0].seq ||
              t == Op::DEL && d2[1].seq == d[1].seq ||
              t == Op::UPD && d2[0].seq == d[0].seq) {
            if (cnt == g_ev_cnt[pk1]) {
              if (! com_op->match) {
                ll2("match pos " << cnt);
                ok = com_op->match = true;
              } else {
                ll2("duplicate match");
              }
            } else {
              ll2("match bad pos event=" << g_ev_cnt[pk1] << " op=" << cnt);
            }
          }
        }
        cnt++;
      }
      com_op = com_op->next_com;
    }
    if (ok)
      return 0;
    ll2("no match");
    if (g_loglevel >= 2)
      return -1;
    loop++;
  }
  return 0;
}

static int
matchevents()
{
  uint nomatch = 0;
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    Op* tot_ev = g_pk_ev[pk1];
    if (tot_ev == 0)
      continue;
    Op* com_ev = tot_ev->next_com;
    while (com_ev != 0) {
      if (matchevent(com_ev) < 0)
        nomatch++;
      g_ev_cnt[pk1]++;
      com_ev = com_ev->next_com;
    }
  }
  chkrc(nomatch == 0);
  return 0;
}

static int
matchops()
{
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    Op* tot_op = g_pk_op[pk1];
    if (tot_op == 0)
      continue;
    Op* com_op = tot_op->next_com;
    while (com_op != 0) {
      if (com_op->type != Op::NUL && ! com_op->match) {
        ll0("COM: " << *com_op);
        Op* op = com_op->next_op;
        assert(op != 0);
        while (op != 0) {
          ll0("---: " << *op);
          op = op->next_op;
        }
        ll0("no matching event");
        return -1;
      }
      com_op = com_op->next_com;
    }
  }
  return 0;
}

static int
runevents()
{
  ll1("runevents");
  NdbEventOperation* evt_op;
  uint npoll = 3;
  while (npoll != 0) {
    npoll--;
    int ret;
    ll1("poll");
    ret = g_ndb->pollEvents(1000);
    if (ret <= 0)
      continue;
    while (1) {
      g_rec_ev->init();
      Data (&d)[2] = g_rec_ev->data;
#ifdef version50
      int overrun = g_opts.maxops;
      chkdb((ret = g_evt_op->next(&overrun)) >= 0);
      chkrc(overrun == 0);
      if (ret == 0)
        break;
#else
      NdbEventOperation* tmp_op = g_ndb->nextEvent();
      if (tmp_op == 0)
        break;
      reqrc(g_evt_op == tmp_op);
#endif
      chkrc(seteventtype(g_rec_ev, g_evt_op->getEventType()) == 0);
      // get indicators
      { int i, j;
        for (j = 0; j < 2; j++)
          for (i = 0; i < g_ncol; i++)
            d[j].ind[i] = g_ra[j][i]->isNULL();
      }
      ll2("runevents: EVT: " << *g_rec_ev);
      // check basic sanity
      Uint32 pk1 = ~(Uint32)0;
      chkrc(checkop(g_rec_ev, pk1) == 0);
      // add to events
      chkrc(getfreeevs() >= 2);
      Op* tot_ev = g_pk_ev[pk1];
      if (tot_ev == 0)
        tot_ev = g_pk_ev[pk1] = getev(); //1
      Op* last_com = tot_ev;
      while (last_com->next_com != 0)
        last_com = last_com->next_com;
      // copy and add
      Op* ev = getev(); //3
      copyop(g_rec_ev, ev);
      last_com->next_com = ev;
    }
  }
  chkrc(matchevents() == 0);
  chkrc(matchops() == 0);
  return 0;
}

static void
setseed(int n)
{
  uint seed;
  if (n == -1) {
    if (g_opts.seed == 0)
      return;
    if (g_opts.seed != -1)
      seed = (uint)g_opts.seed;
    else
      seed = 1 + (ushort)getpid();
  } else {
    if (g_opts.seed != 0)
      return;
    seed = n;
  }
  ll0("seed=" << seed);
  srandom(seed);
}

static int
runtest()
{
  setseed(-1);
  chkrc(createtable() == 0);
  chkrc(createevent() == 0);
  uint n;
  for (n = 0; n < g_opts.loop; n++) {
    ll0("loop " << n);
    setseed(n);
    resetmem();
    g_rec_ev = getev();
    chkrc(createeventop() == 0);
    chkdb(g_evt_op->execute() == 0);
    chkrc(waitgci() == 0);
    makeops();
    chkrc(runops() == 0);
    chkrc(runevents() == 0);
    chkrc(dropeventop() == 0);
  }
  chkrc(dropevent() == 0);
  chkrc(droptable() == 0);
  return 0;
}

NDB_STD_OPTS_VARS;

static struct my_option
my_long_options[] =
{
  NDB_STD_OPTS("test_event_merge"),
  { "abort-on-error", 1008, "Do abort() on any error",
    (gptr*)&g_opts.abort_on_error, (gptr*)&g_opts.abort_on_error, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "loglevel", 1001, "Logging level in this program (default 0)",
    (gptr*)&g_opts.loglevel, (gptr*)&g_opts.loglevel, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "loop", 1002, "Number of test loops (default 1, 0=forever)",
    (gptr*)&g_opts.loop, (gptr*)&g_opts.loop, 0,
    GET_INT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0 },
  { "maxops", 1003, "Number of PK operations (default 2000)",
    (gptr*)&g_opts.maxops, (gptr*)&g_opts.maxops, 0,
    GET_UINT, REQUIRED_ARG, 2000, 0, g_maxops, 0, 0, 0 },
  { "maxpk", 1004, "Number of different PK values (default 10)",
    (gptr*)&g_opts.maxpk, (gptr*)&g_opts.maxpk, 0,
    GET_UINT, REQUIRED_ARG, 10, 1, g_maxpk, 0, 0, 0 },
  { "opstr", 1005, "Ops to run e.g. idiucdc (c = commit, default random)",
    (gptr*)&g_opts.opstr, (gptr*)&g_opts.opstr, 0,
    GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "seed", 1006, "Random seed (0=loop number, default -1=random)",
    (gptr*)&g_opts.seed, (gptr*)&g_opts.seed, 0,
    GET_INT, REQUIRED_ARG, -1, 0, 0, 0, 0, 0 },
  { "separate-events", 1007, "Do not combine events per GCI >5.0",
    (gptr*)&g_opts.separate_events, (gptr*)&g_opts.separate_events, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "use-table", 1008, "Use existing table 'tem1'",
    (gptr*)&g_opts.use_table, (gptr*)&g_opts.use_table, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0,
    0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

static void
usage()
{
  my_print_help(my_long_options);
}

static int
checkopts()
{
  if (g_opts.opstr != 0) {
    const char* s = g_opts.opstr;
    uint n = strlen(s);
    if (n < 3 || s[0] != 'i' || s[n-2] != 'd' || s[n-1] != 'c')
      return -1;
    while (*s != 0)
      if (strchr("iduc", *s++) == 0)
        return -1;
  }
  return 0;
}

int
main(int argc, char** argv)
{
  ndb_init();
  const char* progname =
    strchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
  uint i;
  ndbout << progname;
  for (i = 1; i < argc; i++)
    ndbout << " " << argv[i];
  ndbout << endl;
  int ret;
  ret = handle_options(&argc, &argv, my_long_options, ndb_std_get_one_option);
  if (ret != 0 || argc != 0 || checkopts() != 0)
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  g_ncc = new Ndb_cluster_connection();
  if (g_ncc->connect(30) == 0) {
    g_ndb = new Ndb(g_ncc, "TEST_DB");
    if (g_ndb->init() == 0 && g_ndb->waitUntilReady(30) == 0) {
      if (runtest() == 0)
        return NDBT_ProgramExit(NDBT_OK);
    }
  }
  delete g_ndb;
  delete g_ncc;
  return NDBT_ProgramExit(NDBT_FAILED);
}
