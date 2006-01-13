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

// until rbr in 5.1
#undef version51rbr

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
 * Option --separate-events disables GCI merge and implies --no-blobs.
 * This is used to test basic events functionality.
 *
 * Option --no-blobs omits blob attributes.  This is used to test GCI
 * merge without getting into blob bugs.
 *
 * Option --no-multiops allows 1 operation per commit.  This avoids TUP
 * and blob multi-operation bugs.
 *
 * There are 5 ways (ignoring NUL operand) to compose 2 ops:
 *                      5.0 bugs        5.1 bugs
 * INS o DEL = NUL
 * INS o UPD = INS                      type=INS
 * DEL o INS = UPD      type=INS        type=INS
 * UPD o DEL = DEL      no event
 * UPD o UPD = UPD
 */

struct Opts {
  my_bool abort_on_error;
  int loglevel;
  uint loop;
  uint maxops;
  uint maxpk;
  my_bool no_blobs;
  my_bool no_multiops;
  my_bool one_blob;
  const char* opstring;
  uint seed;
  my_bool separate_events;
  my_bool use_table;
};

static Opts g_opts;
static const uint g_maxpk = 100;
static const uint g_maxopstringpart = 100;
static const char* g_opstringpart[g_maxopstringpart];
static uint g_opstringparts = 0;
static uint g_loop = 0;

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static NdbDictionary::Dictionary* g_dic = 0;
static NdbTransaction* g_con = 0;
static NdbOperation* g_op = 0;
static NdbScanOperation* g_scan_op = 0;

static const char* g_tabname = "tem1";
static const char* g_evtname = "tem1ev1";
static const uint g_charlen = 5;
static const char* g_charval = "abcdefgh";
static const char* g_csname = "latin1_swedish_ci";

static uint g_blobinlinesize = 256;
static uint g_blobpartsize = 2000;
static uint g_blobstripesize = 2;
static const uint g_maxblobsize = 100000;

static const NdbDictionary::Table* g_tab = 0;
static const NdbDictionary::Event* g_evt = 0;

static NdbEventOperation* g_evt_op = 0;
static NdbBlob* g_bh = 0;

static uint
urandom()
{
  uint r = (uint)random();
  return r;
}

static uint
urandom(uint m)
{
  if (m == 0)
    return 0;
  uint r = urandom();
  r = r % m;
  return r;
}

static bool
urandom(uint per, uint cent)
{
  return urandom(cent) < per;
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
  if (g_scan_op != 0) {
    const NdbError& e = g_scan_op->getNdbError();
    if (e.code != 0)
      ll0(++any << " scan_op: error " << e);
  }
  if (g_evt_op != 0) {
    const NdbError& e = g_evt_op->getNdbError();
    if (e.code != 0)
      ll0(++any << " evt_op: error " << e);
  }
  if (g_bh != 0) {
    const NdbError& e = g_bh->getNdbError();
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
  bool isblob() const {
    return type == NdbDictionary::Column::Text;
  }
};

static Col g_col[] = {
  { 0, "pk1", NdbDictionary::Column::Unsigned, true, false, 1, 4 },
  { 1, "pk2", NdbDictionary::Column::Char, true, false,  g_charlen, g_charlen },
  { 2, "seq", NdbDictionary::Column::Unsigned,  false, false, 1, 4 },
  { 3, "cc1", NdbDictionary::Column::Char, false, true, g_charlen, g_charlen },
  { 4, "tx1", NdbDictionary::Column::Text, false, true, 0, 0 },
  { 5, "tx2", NdbDictionary::Column::Text, false, true, 0, 0 }
};

static const uint g_maxcol = sizeof(g_col)/sizeof(g_col[0]);

static uint
ncol()
{
  uint n = g_maxcol;
  if (g_opts.no_blobs)
    n -= 2;
  else if (g_opts.one_blob)
    n -= 1;
  return n;
}

static const Col&
getcol(uint i)
{
  if (i < ncol())
    return g_col[i];
  assert(false);
  return g_col[0];
}

static const Col&
getcol(const char* name)
{
  uint i;
  for (i = 0; i < ncol(); i++)
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
  for (i = 0; i < ncol(); i++) {
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
    case NdbDictionary::Column::Text:
      col.setInlineSize(g_blobinlinesize);
      col.setPartSize(g_blobpartsize);
      col.setStripeSize(g_blobstripesize);
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
    char pk2[g_charlen + 1];
    pk1 = g_maxpk;
    sprintf(pk2, "%-*u", g_charlen, pk1);
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
  struct Txt { char* val; uint len; };
  union Ptr { Uint32* u32; char* ch; Txt* txt; void* v; };
  Uint32 pk1;
  char pk2[g_charlen + 1];
  Uint32 seq;
  char cc1[g_charlen + 1];
  Txt tx1;
  Txt tx2;
  Ptr ptr[g_maxcol];
  int ind[g_maxcol]; // -1 = no data, 1 = NULL, 0 = not NULL
  uint noop; // bit: omit in NdbOperation (implicit NULL INS or no UPD)
  uint ppeq; // bit: post/pre data value equal in GCI data[0]/data[1]
  void init() {
    uint i;
    pk1 = 0;
    memset(pk2, 0, sizeof(pk2));
    seq = 0;
    memset(cc1, 0, sizeof(cc1));
    tx1.val = tx2.val = 0;
    tx1.len = tx2.len = 0;
    ptr[0].u32 = &pk1;
    ptr[1].ch = pk2;
    ptr[2].u32 = &seq;
    ptr[3].ch = cc1;
    ptr[4].txt = &tx1;
    ptr[5].txt = &tx2;
    for (i = 0; i < g_maxcol; i++)
      ind[i] = -1;
    noop = 0;
    ppeq = 0;
  }
  void free() {
    delete [] tx1.val;
    delete [] tx2.val;
    init();
  }
};

static int
cmpcol(const Col& c, const Data& d1, const Data& d2)
{
  uint i = c.no;
  if (d1.ind[i] != d2.ind[i])
    return 1;
  if (d1.ind[i] == 0) {
    switch (c.type) {
    case NdbDictionary::Column::Unsigned:
      if (*d1.ptr[i].u32 != *d2.ptr[i].u32)
        return 1;
      break;
    case NdbDictionary::Column::Char:
      if (memcmp(d1.ptr[i].ch, d2.ptr[i].ch, c.size) != 0)
        return 1;
      break;
    case NdbDictionary::Column::Text:
      {
        const Data::Txt& t1 = *d1.ptr[i].txt;
        const Data::Txt& t2 = *d2.ptr[i].txt;
        if (t1.len != t2.len)
          return 1;
        if (memcmp(t1.val, t2.val, t1.len) != 0)
          return 1;
      }
      break;
    default:
      assert(false);
      break;
    }
  }
  return 0;
}

static NdbOut&
operator<<(NdbOut& out, const Data& d)
{
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col& c = getcol(i);
    out << (i == 0 ? "" : " ") << c.name;
    out << (! (d.noop & (1 << i)) ? "=" : ":");
    if (d.ind[i] == -1)
      continue;
    if (d.ind[i] == 1) {
      out << "NULL";
      continue;
    }
    switch (c.type) {
    case NdbDictionary::Column::Unsigned:
      out << *d.ptr[i].u32;
      break;
    case NdbDictionary::Column::Char:
      {
        char buf[g_charlen + 1];
        memcpy(buf, d.ptr[i].ch, g_charlen);
        uint n = g_charlen;
        while (1) {
          buf[n] = 0;
          if (n == 0 || buf[n - 1] != 0x20)
            break;
          n--;
        }
        out << "'" << buf << "'";
      }
      break;
    case NdbDictionary::Column::Text:
      {
        Data::Txt& t = *d.ptr[i].txt;
        bool first = true;
        uint j = 0;
        while (j < t.len) {
          char c[2];
          c[0] = t.val[j++];
          c[1] = 0;
          uint m = 1;
          while (j < t.len && t.val[j] == c[0])
            j++, m++;
          if (! first)
            out << "+";
          first = false;
          out << m << c;
        }
      }
      break;
    default:
      assert(false);
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
  Op* next_gci; // groups commit chains (unless --separate-events)
  Op* next_ev;
  Op* next_free; // free list
  bool free; // on free list
  uint num_op;
  uint num_com;
  Data data[2]; // 0-post 1-pre
  bool match; // matched to event
  Uint32 gci; // defined for com op and event
  void init(Kind a_kind) {
    kind = a_kind;
    assert(kind == OP || kind == EV);
    type = NUL;
    next_op = next_com = next_gci = next_ev = next_free = 0;
    free = false;
    num_op = num_com = 0;
    data[0].init();
    data[1].init();
    match = false;
    gci = 0;
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
  out << op.type;
  out << " " << op.data[0];
  out << " [" << op.data[1] << "]";
  if (op.gci != 0)
    out << " gci:" << op.gci;
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

static Op* g_opfree = 0;
static uint g_freeops = 0;
static uint g_usedops = 0;
static uint g_maxcom = 10; // max ops per commit
static Op* g_pk_op[g_maxpk];
static Op* g_pk_ev[g_maxpk];
static uint g_seq = 0;
static NdbRecAttr* g_ev_ra[2][g_maxcol]; // 0-post 1-pre
static NdbBlob* g_ev_bh[2][g_maxcol]; // 0-post 1-pre
static Op* g_rec_ev;
static uint g_ev_pos[g_maxpk];

static Op*
getop(Op::Kind a_kind)
{
  if (g_opfree == 0) {
    assert(g_freeops == 0);
    Op* op = new Op;
    assert(op != 0);
    op->next_free = g_opfree;
    g_opfree = op;
    op->free = true;
    g_freeops++;
  }
  Op* op = g_opfree;
  g_opfree = op->next_free;
  assert(g_freeops != 0);
  g_freeops--;
  g_usedops++;
  op->init(a_kind);
  return op;
}

static void
freeop(Op* op)
{
  assert(! op->free);
  op->data[0].free();
  op->data[1].free();
  op->free = true;
  op->next_free = g_opfree;
  g_opfree = op;
  g_freeops++;
  assert(g_usedops != 0);
  g_usedops--;
}

static void
resetmem()
{
  int i, j;
  for (j = 0; j < 2; j++) {
    for (i = 0; i < g_maxcol; i++) {
      g_ev_ra[j][i] = 0;
      g_ev_bh[j][i] = 0;
    }
  }
  if (g_rec_ev != 0) {
    freeop(g_rec_ev);
    g_rec_ev = 0;
  }
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++)
    g_ev_pos[pk1] = 0;
  // leave g_seq
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    if (g_pk_op[pk1] != 0) {
      Op* tot_op = g_pk_op[pk1];
      while (tot_op->next_gci != 0) {
        Op* gci_op = tot_op->next_gci;
        while (gci_op->next_com != 0) {
          Op* com_op = gci_op->next_com;
          while (com_op->next_op != 0) {
            Op* op = com_op->next_op;
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
      g_pk_op[pk1] = 0;
    }
    if (g_pk_ev[pk1] != 0) {
      Op* tot_op = g_pk_ev[pk1];
      while (tot_op->next_ev != 0) {
        Op* ev = tot_op->next_ev;
        tot_op->next_ev = ev->next_ev;
        freeop(ev);
      }
      freeop(tot_op);
      g_pk_ev[pk1] = 0;
    }
  }
  assert(g_usedops == 0);
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
  Op::Type t = op->type;
  if (t == Op::NUL)
    return 0;
  chkrc(t == Op::INS || t == Op::DEL || t == Op::UPD);
  const Data& d0 = op->data[0];
  const Data& d1 = op->data[1];
  {
    const Col& c = getcol("pk1");
    chkrc(d0.ind[c.no] == 0);
    pk1 = d0.pk1;
    chkrc(pk1 < g_opts.maxpk);
  }
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col& c = getcol(i);
    const int ind0 = d0.ind[i];
    const int ind1 = d1.ind[i];
    // the rules are the rules..
    if (c.pk) {
      chkrc(ind0 == 0); // always PK in post data
      if (t == Op::INS)
        chkrc(ind1 == -1);
      if (t == Op::DEL)
        chkrc(ind1 == -1); // no PK in pre data
      if (t == Op::UPD)
        chkrc(ind1 == 0);
    }
    if (! c.pk) {
      if (t == Op::INS)
        chkrc(ind0 >= 0 && ind1 == -1);
      if (t == Op::DEL)
        chkrc(ind0 == -1 && ind1 >= 0); // always non-PK in pre data
      if (t == Op::UPD)
        chkrc(ind0 == -1 || ind1 >= 0); // update must have pre data
    }
    if (! c.nullable) {
      chkrc(ind0 <= 0 && ind1 <= 0);
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
  uint i = c.no;
  if ((d3.ind[i] = d1.ind[i]) == 0) {
    if (! c.isblob()) {
      memmove(d3.ptr[i].v, d1.ptr[i].v, c.size);
    } else {
      Data::Txt& t1 = *d1.ptr[i].txt;
      Data::Txt& t3 = *d3.ptr[i].txt;
      delete [] t3.val;
      t3.val = new char [t1.len];
      t3.len = t1.len;
      memcpy(t3.val, t1.val, t1.len);
    }
  }
}

static void
copydata(const Data& d1, Data& d3, bool pk, bool nonpk)
{
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col& c = g_col[i];
    if (c.pk && pk || ! c.pk && nonpk)
      copycol(c, d1, d3);
  }
}

static void
compdata(const Data& d1, const Data& d2, Data& d3, bool pk, bool nonpk)
{
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col& c = g_col[i];
    if (c.pk && pk || ! c.pk && nonpk) {
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
}

static void
copyop(const Op* op1, Op* op3)
{
  op3->type = op1->type;
  copydata(op1->data[0], op3->data[0], true, true);
  copydata(op1->data[1], op3->data[1], true, true);
  op3->gci = op1->gci;
  Uint32 pk1_tmp;
  reqrc(checkop(op3, pk1_tmp) == 0);
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
  Op::Kind kind =
    op1->kind == Op::OP && op2->kind == Op::OP ? Op::OP : Op::EV;
  Op* res_op = getop(kind);
  chkrc((comp = comptype(op1->type, op2->type)) != 0);
  res_op->type = comp->t3;
  if (res_op->type == Op::INS) {
    // INS o UPD
    compdata(op1->data[0], op2->data[0], res_op->data[0], true, true);
    // pre = undef
  }
  if (res_op->type == Op::DEL) {
    // UPD o DEL
    copydata(op2->data[0], res_op->data[0], true, false); // PK
    copydata(op1->data[1], res_op->data[1], false, true); // non-PK
  } 
  if (res_op->type == Op::UPD && op1->type == Op::DEL) {
    // DEL o INS
    copydata(op2->data[0], res_op->data[0], true, true);
    copydata(op1->data[0], res_op->data[1], true, false); // PK
    copydata(op1->data[1], res_op->data[1], false, true); // non-PK
  }
  if (res_op->type == Op::UPD && op1->type == Op::UPD) {
    // UPD o UPD
    compdata(op1->data[0], op2->data[0], res_op->data[0], true, true);
    compdata(op2->data[1], op1->data[1], res_op->data[1], true, true);
  }
  assert(op1->gci == op2->gci);
  res_op->gci = op2->gci;
  Uint32 pk1_tmp;
  reqrc(checkop(res_op, pk1_tmp) == 0);
  copyop(res_op, op3);
  freeop(res_op);
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
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col& c = g_col[i];
    evt.addEventColumn(c.name);
  }
#ifdef version51rbr
  evt.separateEvents(g_opts.separate_events);
#endif
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
#ifdef version51rbr
  g_evt_op->separateEvents(g_opts.separate_events); // not yet inherited
#endif
#endif
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col& c = g_col[i];
    Data (&d)[2] = g_rec_ev->data;
    if (! c.isblob()) {
      chkdb((g_ev_ra[0][i] = g_evt_op->getValue(c.name, (char*)d[0].ptr[i].v)) != 0);
      chkdb((g_ev_ra[1][i] = g_evt_op->getPreValue(c.name, (char*)d[1].ptr[i].v)) != 0);
    } else {
#ifdef version51rbr
      chkdb((g_ev_bh[0][i] = g_evt_op->getBlobHandle(c.name)) != 0);
      chkdb((g_ev_bh[1][i] = g_evt_op->getPreBlobHandle(c.name)) != 0);
#endif
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
      char pk2[g_charlen + 1];
      pk1 = g_maxpk;
      sprintf(pk2, "%-*u", g_charlen, pk1);
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
    sleep(1);
  }
  return 0;
}

// scan table and set current tot_op for each pk1
static int
scantab()
{
  NdbRecAttr* ra[g_maxcol];
  NdbBlob* bh[g_maxcol];
  Op* rec_op = getop(Op::OP);
  Data& d0 = rec_op->data[0];
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  chkdb((g_scan_op = g_con->getNdbScanOperation(g_tabname)) != 0);
  chkdb(g_scan_op->readTuples() == 0);
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col& c = getcol(i);
    if (! c.isblob()) {
      chkdb((ra[i] = g_scan_op->getValue(c.name, (char*)d0.ptr[i].v)) != 0);
    } else {
      chkdb((bh[i] = g_scan_op->getBlobHandle(c.name)) != 0);
    }
  }
  chkdb(g_con->execute(NoCommit) == 0);
  int ret;
  while ((ret = g_scan_op->nextResult()) == 0) {
    Uint32 pk1 = d0.pk1;
    if (pk1 >= g_opts.maxpk)
      continue;
    rec_op->type = Op::INS;
    for (i = 0; i < ncol(); i++) {
      const Col& c = getcol(i);
      int ind;
      if (! c.isblob()) {
        ind = ra[i]->isNULL();
      } else {
#ifdef version51rbr
        int ret;
        ret = bh[i]->getDefined(ind);
        assert(ret == 0);
        if (ind == 0) {
          Data::Txt& t = *d0.ptr[i].txt;
          Uint64 len64;
          ret = bh[i]->getLength(len64);
          assert(ret == 0);
          t.len = (uint)len64;
          delete [] t.val;
          t.val = new char [t.len];
          memset(t.val, 'X', t.len);
          Uint32 len = t.len;
          ret = bh[i]->readData(t.val, len);
          assert(ret == 0 && len == t.len);
        }
#endif
      }
      assert(ind >= 0);
      d0.ind[i] = ind;
    }
    assert(g_pk_op[pk1] == 0);
    Op* tot_op = g_pk_op[pk1] = getop(Op::OP);
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

static void
makedata(const Col& c, Data& d, Uint32 pk1, Op::Type t)
{
  uint i = c.no;
  if (c.pk) {
    switch (c.type) {
    case NdbDictionary::Column::Unsigned:
      {
        Uint32* p = d.ptr[i].u32;
        *p = pk1;
      }
      break;
    case NdbDictionary::Column::Char:
      {
        char* p = d.ptr[i].ch;
        sprintf(p, "%-*u", g_charlen, pk1);
      }
      break;
    default:
      assert(false);
      break;
    }
    d.ind[i] = 0;
  } else if (t == Op::DEL) {
    ;
  } else if (i == getcol("seq").no) {
    d.seq = g_seq++;
    d.ind[i] = 0;
  } else if (t == Op::INS && c.nullable && urandom(10, 100)) {
    d.noop |= (1 << i);
    d.ind[i] = 1; // implicit NULL value is known
  } else if (t == Op::UPD && urandom(10, 100)) {
    d.noop |= (1 << i);
    d.ind[i] = -1; // fixed up in caller
  } else if (c.nullable && urandom(10, 100)) {
    d.ind[i] = 1;
  } else {
    switch (c.type) {
    case NdbDictionary::Column::Unsigned:
      {
        Uint32* p = d.ptr[i].u32;
        uint u = urandom();
        *p = u;
      }
      break;
    case NdbDictionary::Column::Char:
      {
        char* p = d.ptr[i].ch;
        uint u = urandom(g_charlen);
        uint j;
        for (j = 0; j < g_charlen; j++) {
          uint v = urandom(strlen(g_charval));
          p[j] = j < u ? g_charval[v] : 0x20;
        }
      }
      break;
    case NdbDictionary::Column::Text:
      {
        Data::Txt& t = *d.ptr[i].txt;
        uint u = urandom(g_maxblobsize);
        u = urandom(u); // 4x bias for smaller blobs
        u = urandom(u);
        delete [] t.val;
        t.val = new char [u];
        t.len = u;
        uint j = 0;
        while (j < u) {
          assert(u > 0);
          uint k = 1 + urandom(u - 1);
          if (k > u - j)
            k = u - j;
          uint v = urandom(strlen(g_charval));
          memset(&t.val[j], g_charval[v], k);
          j += k;
        }
      }
      break;
    default:
      assert(false);
      break;
    }
    d.ind[i] = 0;
  }
}

static void
makeop(const Op* prev_op, Op* op, Uint32 pk1, Op::Type t)
{
  op->type = t;
  const Data& dp = prev_op->data[0];
  Data& d0 = op->data[0];
  Data& d1 = op->data[1];
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col& c = getcol(i);
    makedata(c, d0, pk1, t);
    if (t == Op::INS) {
      d1.ind[i] = -1;
    } else if (t == Op::DEL) {
      assert(dp.ind[i] >= 0);
      if (c.pk)
        d1.ind[i] = -1;
      else
        copycol(c, dp, d1);
    } else if (t == Op::UPD) {
      assert(dp.ind[i] >= 0);
      if (d0.ind[i] == -1) // not updating this col
        copycol(c, dp, d0); // must keep track of data
      copycol(c, dp, d1);
    } else {
      assert(false);
    }
  }
  Uint32 pk1_tmp = ~(Uint32)0;
  reqrc(checkop(op, pk1_tmp) == 0);
  reqrc(pk1 == pk1_tmp);
}

static void
makeops()
{
  ll1("makeops");
  Uint32 pk1 = 0;
  while (g_usedops < g_opts.maxops && pk1 < g_opts.maxpk) {
    if (g_opts.opstring == 0)
      pk1 = urandom(g_opts.maxpk);
    ll2("makeops: pk1=" << pk1);
    // total op on the pk so far
    // optype either NUL=initial/deleted or INS=created
    Op* tot_op = g_pk_op[pk1];
    if (tot_op == 0)
      tot_op = g_pk_op[pk1] = getop(Op::OP);
    assert(tot_op->type == Op::NUL || tot_op->type == Op::INS);
    // add new commit chain to end
    Op* last_gci = tot_op;
    while (last_gci->next_gci != 0)
      last_gci = last_gci->next_gci;
    Op* gci_op = getop(Op::OP);
    last_gci->next_gci = gci_op;
    Op* com_op = getop(Op::OP);
    gci_op->next_com = com_op;
    // length of random chain
    uint len = ~0;
    if (g_opts.opstring == 0) {
      len = 1 + urandom(g_maxcom - 1);
      len = 1 + urandom(len - 1); // 2x bias for short chain
    }
    ll2("makeops: com chain");
    uint n = 0;
    while (1) {
      // random or from current g_opts.opstring part
      Op::Type t;
      if (g_opts.opstring == 0) {
        if (n == len)
          break;
        do {
          t = (Op::Type)urandom(g_optypes);
        } while (tot_op->type == Op::NUL && (t == Op::DEL || t == Op::UPD) ||
                 tot_op->type == Op::INS && t == Op::INS);
      } else {
        const char* str = g_opstringpart[g_loop % g_opstringparts];
        uint m = strlen(str);
        uint k = tot_op->num_com + tot_op->num_op;
        assert(k < m);
        char c = str[k];
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
      Op* op = getop(Op::OP);
      makeop(tot_op, op, pk1, t);
      // add to end
      Op* last_op = com_op;
      while (last_op->next_op != 0)
        last_op = last_op->next_op;
      last_op->next_op = op;
      // merge into chain head and total op
      reqrc(compop(com_op, op, com_op) == 0);
      reqrc(compop(tot_op, op, tot_op) == 0);
      assert(tot_op->type == Op::NUL || tot_op->type == Op::INS);
      // counts
      com_op->num_op += 1;
      tot_op->num_op += 1;
      n++;
    }
    // copy to gci level
    copyop(com_op, gci_op);
    tot_op->num_com += 1;
  }
  ll1("makeops: used ops = " << g_usedops);
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
  for (i = 0; i < ncol(); i++) {
    const Col& c = getcol(i);
    const Data& d = op->data[0];
    if (! c.pk)
      continue;
    chkdb(g_op->equal(c.name, (const char*)d.ptr[i].v) == 0);
  }
  if (op->type != Op::DEL) {
    for (i = 0; i < ncol(); i++) {
      const Col& c = getcol(i);
      const Data& d = op->data[0];
      if (c.pk)
        continue;
      if (d.noop & (1 << i))
        continue;
      assert(d.ind[i] >= 0);
      if (! c.isblob()) {
        if (d.ind[i] == 0)
          chkdb(g_op->setValue(c.name, (const char*)d.ptr[i].v) == 0);
        else
          chkdb(g_op->setValue(c.name, (const char*)0) == 0);
      } else {
        const Data::Txt& t = *d.ptr[i].txt;
        g_bh = g_op->getBlobHandle(c.name);
        if (d.ind[i] == 0)
          chkdb(g_bh->setValue(t.val, t.len) == 0);
        else
          chkdb(g_bh->setValue(0, 0) == 0);
        g_bh = 0;
      }
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
  Op* gci_op[g_maxpk];
  uint left = 0; // number of pks with ops
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    gci_op[pk1] = 0;
    // total op on the pk
    Op* tot_op = g_pk_op[pk1];
    if (tot_op == 0)
      continue;
    // first commit chain
    assert(tot_op->next_gci != 0);
    gci_op[pk1] = tot_op->next_gci;
    left++;
  }
  while (left != 0) {
    pk1 = urandom(g_opts.maxpk);
    if (gci_op[pk1] == 0)
      continue;
    // do the ops in one transaction
    chkdb((g_con = g_ndb->startTransaction()) != 0);
    Op* com_op = gci_op[pk1]->next_com;
    assert(com_op != 0);
    // first op in chain
    Op* op = com_op->next_op;
    assert(op != 0);
    while (op != 0) {
      ll2("runops:" << *op);
      chkrc(addndbop(op) == 0);
      op = op->next_op;
    }
    chkdb(g_con->execute(Commit) == 0);
    gci_op[pk1]->gci = com_op->gci = g_con->getGCI();
    ll2("commit: gci=" << com_op->gci);
    g_ndb->closeTransaction(g_con);
    g_con = 0;
    // next chain
    gci_op[pk1] = gci_op[pk1]->next_gci;
    if (gci_op[pk1] == 0) {
      assert(left != 0);
      left--;
    }
  }
  assert(left == 0);
  return 0;
}

// move com chains with same gci under same gci entry
static int
mergeops()
{
  ll1("mergeops");
  uint mergecnt = 0;
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    Op* tot_op = g_pk_op[pk1];
    if (tot_op == 0)
      continue;
    Op* gci_op = tot_op->next_gci;
    assert(gci_op != 0);
    while (gci_op != 0) {
      Op* com_op = gci_op->next_com;
      assert(com_op != 0 && com_op->next_com == 0);
      assert(gci_op->gci == com_op->gci);
      Op* last_com = com_op;
      Op* gci_op2 = gci_op->next_gci;
      while (gci_op2 != 0 && gci_op->gci == gci_op2->gci) {
        // move link to com level
        last_com = last_com->next_com = gci_op2->next_com;
        // merge to gci
        reqrc(compop(gci_op, gci_op2, gci_op) == 0);
        // move to next and discard
        Op* tmp_op = gci_op2;
        gci_op2 = gci_op2->next_gci;
        freeop(tmp_op);
        mergecnt++;
      }
      gci_op = gci_op->next_gci = gci_op2;
    }
  }
  ll1("mergeops: used ops = " << g_usedops);
  ll1("mergeops: merged " << mergecnt << " gci entries");
  return 0;
}

// set bit for equal post/pre data in UPD, for use in event match
static void
cmppostpre()
{
  ll1("cmppostpre");
  Uint32 pk1;
  for (pk1 = 0; pk1 < g_opts.maxpk; pk1++) {
    Op* tot_op = g_pk_op[pk1];
    Op* gci_op = tot_op ? tot_op->next_gci : 0;
    while (gci_op != 0) {
      if (gci_op->type == Op::UPD) {
        Data (&d)[2] = gci_op->data;
        uint i;
        for (i = 0; i < ncol(); i++) {
          const Col& c = getcol(i);
          bool eq =
            d[0].ind[i] == 1 && d[1].ind[i] == 1 ||
            d[0].ind[i] == 0 && d[1].ind[i] == 0 && cmpcol(c, d[0], d[1]) == 0;
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
static int
cmpopevdata(const Data& d1, const Data& d2)
{
  uint i;
  for (i = 0; i < ncol(); i++) {
    const Col& c = getcol(i);
    if (cmpcol(c, d1, d2) != 0) {
      if ((d1.ppeq & (1 << i)) && d2.ind[i] == -1)
        ; // post/pre data equal and no event data returned is OK
      else
        return 1;
    }
  }
  return 0;
}

// compare operation to event data
static int
cmpopevdata(const Data (&d1)[2], const Data (&d2)[2])
{
  if (cmpopevdata(d1[0], d2[0]) != 0)
    return 1;
  if (cmpopevdata(d1[1], d2[1]) != 0)
    return 1;
  return 0;
}

static int
matchevent(Op* ev)
{
  Op::Type t = ev->type;
  Data (&d2)[2] = ev->data;
  // get PK
  Uint32 pk1 = d2[0].pk1;
  chkrc(pk1 < g_opts.maxpk);
  // on error repeat and print details
  uint loop = 0;
  while (loop <= 1) {
    uint g_loglevel = loop == 0 ? g_opts.loglevel : 2;
    ll1("matchevent: pk1=" << pk1 << " type=" << t);
    ll2("EVT: " << *ev);
    Op* tot_op = g_pk_op[pk1];
    Op* gci_op = tot_op ? tot_op->next_gci : 0;
    uint pos = 0;
    bool ok = false;
    while (gci_op != 0) {
      ll2("GCI: " << *gci_op);
      // print details
      Op* com_op = gci_op->next_com;
      assert(com_op != 0);
      while (com_op != 0) {
        ll2("COM: " << *com_op);
        Op* op = com_op->next_op;
        assert(op != 0);
        while (op != 0) {
          ll2("OP : " << *op);
          op = op->next_op;
        }
        com_op = com_op->next_com;
      }
      // match agains GCI op
      if (gci_op->type != Op::NUL) {
        const Data (&d1)[2] = gci_op->data;
        if (cmpopevdata(d1, d2) == 0) {
          bool tmpok = true;
          if (gci_op->type != t) {
            ll2("***: wrong type " << gci_op->type << " != " << t);
            tmpok = false;
          }
          if (gci_op->match) {
            ll2("***: duplicate match");
            tmpok = false;
          }
          if (pos != g_ev_pos[pk1]) {
            ll2("***: wrong pos " << pos << " != " << g_ev_pos[pk1]);
            tmpok = false;
          }
          if (gci_op->gci != ev->gci) {
            ll2("***: wrong gci " << gci_op->gci << " != " << ev->gci);
            tmpok = false;
          }
          if (tmpok) {
            ok = gci_op->match = true;
            ll2("===: match");
          }
        }
        pos++;
      }
      gci_op = gci_op->next_gci;
    }
    if (ok) {
      ll1("matchevent: match");
      return 0;
    }
    ll1("matchevent: ERROR: no match");
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
    Op* ev = tot_ev->next_ev;
    while (ev != 0) {
      if (matchevent(ev) < 0)
        nomatch++;
      g_ev_pos[pk1]++;
      ev = ev->next_ev;
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

static void
geteventdata()
{
  Data (&d)[2] = g_rec_ev->data;
  int i, j;
  for (j = 0; j < 2; j++) {
    for (i = 0; i < ncol(); i++) {
      const Col& c = getcol(i);
      int ind, ret;
      if (! c.isblob()) {
        NdbRecAttr* ra = g_ev_ra[j][i];
        ind = ra->isNULL();
      } else {
#ifdef version51rbr
        NdbBlob* bh = g_ev_bh[j][i];
        ret = bh->getDefined(ind);
        assert(ret == 0);
        if (ind == 0) { // value was returned and is not NULL
          Data::Txt& t = *d[j].ptr[i].txt;
          Uint64 len64;
          ret = bh->getLength(len64);
          assert(ret == 0);
          t.len = (uint)len64;
          delete [] t.val;
          t.val = new char [t.len];
          memset(t.val, 'X', t.len);
          Uint32 len = t.len;
          ret = bh->readData(t.val, len);
          assert(ret == 0 && len == t.len);
        }
#endif
      }
      d[j].ind[i] = ind;
    }
  }
}

static int
runevents()
{
  ll1("runevents");
  uint mspoll = 1000;
  uint npoll = 6; // strangely long delay
  while (npoll != 0) {
    npoll--;
    int ret;
    ll1("poll");
    ret = g_ndb->pollEvents(mspoll);
    if (ret <= 0)
      continue;
    while (1) {
      g_rec_ev->init(Op::EV);
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
      geteventdata();
      g_rec_ev->gci = g_evt_op->getGCI();
#ifdef version50
      // fix to match 5.1
      if (g_rec_ev->type == Op::UPD) {
        Uint32 pk1 = g_rec_ev->data[0].pk1;
        makedata(getcol("pk1"), g_rec_ev->data[1], pk1, Op::UPD);
        makedata(getcol("pk2"), g_rec_ev->data[1], pk1, Op::UPD);
      }
#endif
      // get indicators and blob value
      ll2("runevents: EVT: " << *g_rec_ev);
      // check basic sanity
      Uint32 pk1 = ~(Uint32)0;
      chkrc(checkop(g_rec_ev, pk1) == 0);
      // add to events
      Op* tot_ev = g_pk_ev[pk1];
      if (tot_ev == 0)
        tot_ev = g_pk_ev[pk1] = getop(Op::EV);
      Op* last_ev = tot_ev;
      while (last_ev->next_ev != 0)
        last_ev = last_ev->next_ev;
      // copy and add
      Op* ev = getop(Op::EV);
      copyop(g_rec_ev, ev);
      last_ev->next_ev = ev;
    }
  }
  ll1("runevents: used ops = " << g_usedops);
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
  for (g_loop = 0; g_opts.loop == 0 || g_loop < g_opts.loop; g_loop++) {
    ll0("loop " << g_loop);
    setseed(g_loop);
    resetmem();
    chkrc(scantab() == 0); // alternative: save tot_op for loop > 0
    makeops();
    g_rec_ev = getop(Op::EV);
    chkrc(createeventop() == 0);
    chkdb(g_evt_op->execute() == 0);
    chkrc(waitgci() == 0);
    chkrc(runops() == 0);
    if (! g_opts.separate_events)
      chkrc(mergeops() == 0);
    cmppostpre();
    chkrc(runevents() == 0);
    chkrc(matchevents() == 0);
    chkrc(matchops() == 0);
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
  { "abort-on-error", 1001, "Do abort() on any error",
    (gptr*)&g_opts.abort_on_error, (gptr*)&g_opts.abort_on_error, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "loglevel", 1002, "Logging level in this program (default 0)",
    (gptr*)&g_opts.loglevel, (gptr*)&g_opts.loglevel, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "loop", 1003, "Number of test loops (default 2, 0=forever)",
    (gptr*)&g_opts.loop, (gptr*)&g_opts.loop, 0,
    GET_INT, REQUIRED_ARG, 2, 0, 0, 0, 0, 0 },
  { "maxops", 1004, "Approx number of PK operations (default 1000)",
    (gptr*)&g_opts.maxops, (gptr*)&g_opts.maxops, 0,
    GET_UINT, REQUIRED_ARG, 1000, 0, 0, 0, 0, 0 },
  { "maxpk", 1005, "Number of different PK values (default 10)",
    (gptr*)&g_opts.maxpk, (gptr*)&g_opts.maxpk, 0,
    GET_UINT, REQUIRED_ARG, 10, 1, g_maxpk, 0, 0, 0 },
  { "no-blobs", 1006, "Omit blob attributes (5.0: true)",
    (gptr*)&g_opts.no_blobs, (gptr*)&g_opts.no_blobs, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "no-multiops", 1007, "Allow only 1 operation per commit",
    (gptr*)&g_opts.no_multiops, (gptr*)&g_opts.no_multiops, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "one-blob", 1008, "Only one blob attribute (defautt 2)",
    (gptr*)&g_opts.one_blob, (gptr*)&g_opts.one_blob, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "opstring", 1009, "Operations to run e.g. idiucdc (c is commit) or"
                      " iuuc:uudc (the : separates loops)",
    (gptr*)&g_opts.opstring, (gptr*)&g_opts.opstring, 0,
    GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "seed", 1010, "Random seed (0=loop number, default -1=random)",
    (gptr*)&g_opts.seed, (gptr*)&g_opts.seed, 0,
    GET_INT, REQUIRED_ARG, -1, 0, 0, 0, 0, 0 },
  { "separate-events", 1011, "Do not combine events per GCI (5.0: true)",
    (gptr*)&g_opts.separate_events, (gptr*)&g_opts.separate_events, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "use-table", 1012, "Use existing table 'tem1'",
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
#ifdef version50
  g_opts.separate_events = true;
#endif
  if (g_opts.separate_events) {
    g_opts.no_blobs = true;
  }
  if (g_opts.no_multiops) {
    g_maxcom = 1;
  }
  if (g_opts.opstring != 0) {
    uint len = strlen(g_opts.opstring);
    char* str = new char [len + 1];
    memcpy(str, g_opts.opstring, len + 1);
    char* s = str;
    while (1) {
      g_opstringpart[g_opstringparts++] = s;
      s = strchr(s, ':');
      if (s == 0)
        break;
      *s++ = 0;
    }
    uint i;
    for (i = 0; i < g_opstringparts; i++) {
      const char* s = g_opstringpart[i];
      while (*s != 0)
        if (strchr("iduc", *s++) == 0)
          return -1;
      if (s == g_opstringpart[i] || s[-1] != 'c')
        return -1;
    }
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
  if (g_evt_op != 0) {
    (void)dropeventop();
    g_evt_op = 0;
  }
  delete g_ndb;
  delete g_ncc;
  return NDBT_ProgramExit(NDBT_FAILED);
}
