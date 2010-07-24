/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
#include <NdbIndexStat.hpp>
#include <NdbTest.hpp>
#include <my_sys.h>
#include <ndb_version.h>
#include <math.h>

/*
 * Sample results:
 *
 * 0. err pct: count: 1000 min: -99.99 max: 99.92 avg: 6.88 stddev: 27.61
 *
 * 0. baseline with same options as handler
 */

#undef min
#undef max
#define min(a, b) ((a) <= (b) ? (a) : (b))
#define max(a, b) ((a) >= (b) ? (a) : (b))

inline NdbOut&
NdbOut::operator<<(double x)
{
  char buf[100];
  sprintf(buf, "%.2f", x);
  *this << buf;
  return *this;
}

struct Opts {
  int loglevel;
  uint seed;
  uint loop;
  uint rows;
  uint ops;
  uint nullkeys;
  uint dupkeys;
  uint scanpct;
  uint eqscans;
  uint dupscans;
  my_bool keeptable;
  my_bool loaddata;
  my_bool nochecks;
  my_bool abort;
  // internal
  uint tryhard;
  Opts() :
    loglevel(0),
    seed(-1),
    loop(1),
    rows(100000),
    ops(1000),
    nullkeys(10),
    dupkeys(1000),
    scanpct(5),
    eqscans(50),
    dupscans(10),
    keeptable(false),
    loaddata(true),
    nochecks(false),
    abort(false),
    // internal
    tryhard(20)
  {}
};

static Opts g_opts;
const char* g_progname = "testIndexStat";
static uint g_loop = 0;

static const char* g_tabname = "ts0";
static const char* g_indname = "ts0x1";
static const char g_numattrs = 3;
static const uint g_charlen = 10;
static const char* g_csname = "latin1_swedish_ci";
static CHARSET_INFO* g_cs;

// value and bound ranges
static uint g_val_b_max = 10;
static uint g_bnd_b_max = 20;
static const char* g_val_c_char = "bcd";
static const char* g_bnd_c_char = "abcde";
static uint g_val_d_max = 100;
static uint g_bnd_d_max = 200;

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static NdbDictionary::Dictionary* g_dic = 0;
static const NdbDictionary::Table* g_tab = 0;
static const NdbDictionary::Index* g_ind = 0;

static NdbIndexStat* g_stat = 0;

static NdbTransaction* g_con = 0;
static NdbOperation* g_op = 0;
static NdbScanOperation* g_scan_op = 0;
static NdbIndexScanOperation* g_rangescan_op = 0;

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

static int& g_loglevel = g_opts.loglevel; // default log level

#define chkdb(x) \
  do { if (likely(x)) break; ndbout << "line " << __LINE__ << " FAIL " << #x << endl; errdb(); if (g_opts.abort) abort(); return -1; } while (0)

#define chkrc(x) \
  do { if (likely(x)) break; ndbout << "line " << __LINE__ << " FAIL " << #x << endl; if (g_opts.abort) abort(); return -1; } while (0)

#define reqrc(x) \
  do { if (likely(x)) break; ndbout << "line " << __LINE__ << " ASSERT " << #x << endl; abort(); } while (0)

#define llx(n, x) \
  do { if (likely(g_loglevel < n)) break; ndbout << x << endl; } while (0)

#define ll0(x) llx(0, x)
#define ll1(x) llx(1, x)
#define ll2(x) llx(2, x)
#define ll3(x) llx(3, x)

static void
errdb()
{
  uint any = 0;
  // g_ncc return no error...
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
  if (g_rangescan_op != 0) {
    const NdbError& e = g_rangescan_op->getNdbError();
    if (e.code != 0)
      ll0(++any << " rangescan_op: error " << e);
  }
  if (g_stat != 0) {
    const NdbError& e = g_stat->getNdbError();
    if (e.code != 0)
      ll0(++any << " stat: error " << e);
  }
  if (! any)
    ll0("unknown db error");
}

// create table ts0 (
//   a int unsigned, b smallint not null, c varchar(10), d int unsigned,
//   primary key using hash (a), index (b, c, d) )

static int
createtable()
{
  NdbDictionary::Table tab(g_tabname);
  tab.setLogging(false);
  {
    NdbDictionary::Column col("a");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("b");
    col.setType(NdbDictionary::Column::Smallint);
    col.setNullable(false);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("c");
    col.setType(NdbDictionary::Column::Varchar);
    col.setLength(g_charlen);
    col.setCharset(g_cs);
    col.setNullable(true);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("d");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(true);
    tab.addColumn(col);
  }
  NdbDictionary::Index ind(g_indname);
  ind.setTable(g_tabname);
  ind.setType(NdbDictionary::Index::OrderedIndex);
  ind.setLogging(false);
  ind.addColumnName("b");
  ind.addColumnName("c");
  ind.addColumnName("d");
  g_dic = g_ndb->getDictionary();
  if (! g_opts.keeptable) {
    if (g_dic->getTable(g_tabname) != 0)
      chkdb(g_dic->dropTable(g_tabname) == 0);
    chkdb(g_dic->createTable(tab) == 0);
    chkdb(g_dic->createIndex(ind) == 0);
  } else {
    if (g_dic->getTable(g_tabname) == 0) {
      chkdb(g_dic->createTable(tab) == 0);
      chkdb(g_dic->createIndex(ind) == 0);
    } else
      g_opts.loaddata = false;
  }
  chkdb((g_tab = g_dic->getTable(g_tabname)) != 0);
  chkdb((g_ind = g_dic->getIndex(g_indname, g_tabname)) != 0);
  g_dic = 0;
  return 0;
}

static int
droptable()
{
  g_dic = g_ndb->getDictionary();
  if (! g_opts.keeptable)
    chkdb(g_dic->dropTable(g_tabname) == 0);
  g_dic = 0;
  return 0;
}

struct Val {
  Int16 b;
  bool c_null;
  uchar c[1 + g_charlen];
  bool d_null;
  Uint32 d;
  // partial values for use in Bnd
  uint numattrs;
  void make(uint n = g_numattrs, bool is_val = true);
  int cmp(const Val& val, uint n = g_numattrs) const;
};

static NdbOut&
operator<<(NdbOut& out, const Val& val)
{
  out << "[";
  if (val.numattrs >= 1) {
    out << val.b;
  }
  if (val.numattrs >= 2) {
    out << " ";
    if (val.c_null)
      out << "NULL";
    else {
      char buf[1 + g_charlen];
      sprintf(buf, "%.*s", val.c[0], &val.c[1]);
      out << "'" << buf << "'";
    }
  }
  if (val.numattrs >= 3) {
    out << " ";
    if (val.d_null)
      out <<" NULL";
    else
      out << val.d;
  }
  out << "]";
  return out;
}

void
Val::make(uint n, bool is_val)
{
  if (n >= 1) {
    uint b_max = is_val ? g_val_b_max : g_bnd_b_max;
    b = (int)urandom(2 * b_max) - (int)b_max;
  }
  if (n >= 2) {
    if (urandom(100) < g_opts.nullkeys)
      c_null = 1;
    else {
      const char* c_char = is_val ? g_val_c_char : g_bnd_c_char;
      // prefer shorter
      uint len = urandom(urandom(g_charlen + 2));
      c[0] = len;
      uint j;
      for (j = 0; j < len; j++) {
        uint k = urandom(strlen(c_char));
        c[1 + j] = c_char[k];
      }
      c_null = 0;
    }
  }
  if (n >= 3) {
    if (urandom(100) < g_opts.nullkeys)
      d_null = 1;
    else {
      uint d_max = is_val ? g_val_d_max : g_bnd_d_max;
      d = urandom(d_max);
      d_null = 0;
    }
  }
  numattrs = n;
}

int
Val::cmp(const Val& val, uint n) const
{
  int k = 0;
  if (k == 0 && n >= 1) {
    if (b < val.b)
      k = -1;
    else if (b > val.b)
      k = +1;
  }
  if (k == 0 && n >= 2) {
    if (! c_null && ! val.c_null) {
      const uchar* s1 = &c[1];
      const uchar* s2 = &val.c[1];
      const uint l1 = (uint)c[0];
      const uint l2 = (uint)val.c[0];
      assert(l1 <= g_charlen && l2 <= g_charlen);
      k = g_cs->coll->strnncollsp(g_cs, s1, l1, s2, l2, 0);
    } else if (! c_null) {
      k = +1;
    } else if (! val.c_null) {
      k = -1;
    }
  }
  if (k == 0 && n >= 3) {
    if (! d_null && ! val.d_null) {
      if (d < val.d)
        k = -1;
      else if (d > val.d)
        k = +1;
    } else if (! d_null) {
      k = +1;
    } else if (! val.d_null) {
      k = -1;
    }
  }
  return k;
}

struct Key {
  Val val;
  union {
    bool flag;
    uint count;
    uint rpk;
  };
};

static NdbOut&
operator<<(NdbOut& out, const Key& key)
{
  out << key.val << " info:" << key.count;
  return out;
}

static Key* g_keys = 0;
static Key* g_sortkeys = 0;
static uint g_sortcount = 0;
static Key* g_minkey = 0;
static Key* g_maxkey = 0;

static void
freekeys()
{
  if (g_keys != 0)
    my_free(g_keys);
  if (g_sortkeys != 0)
    my_free(g_sortkeys);
  g_keys = 0;
  g_sortkeys = 0;
}

static int
allockeys()
{
  freekeys();
  size_t sz = sizeof(Key) * g_opts.rows;
  g_keys = (Key*)my_malloc(sz, MYF(0));
  g_sortkeys = (Key*)my_malloc(sz, MYF(0));
  chkrc(g_keys != 0 && g_sortkeys != 0);
  memset(g_keys, 0x1f, sz);
  memset(g_sortkeys, 0x1f, sz);
  return 0;
}

static void
makekeys()
{
  uint i;
  for (i = 0; i < g_opts.rows; i++) {
    Key& key = g_keys[i];
    key.val.make();
    key.flag = false; // mark for dup generation done
  }
  for (i = 0; i < g_opts.rows; i++) {
    Key& key = g_keys[i];
    if (key.flag)
      continue;
    key.flag = true;
    uint fudge = 9;
    uint n = (urandom(fudge * (g_opts.dupkeys - 100)) + 99) / 100;
    uint k;
    for (k = 1; k < n; k++) {
      uint j = urandom(g_opts.rows);
      do {
        Key& dst = g_keys[j];
        if (! dst.flag) {
          dst.val = key.val;
          dst.flag = true;
          break;
        }
      } while (urandom(g_opts.tryhard) != 0);
    }
  }
}

static int
insertdata()
{
  const uint batch = 512;
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  uint i = 0;
  while (i < g_opts.rows) {
    chkdb((g_op = g_con->getNdbOperation(g_tab)) != 0);
    chkdb(g_op->insertTuple() == 0);
    Uint32 a = i;
    const Val& val = g_keys[i].val;
    const char* a_addr = (const char*)&a;
    const char* b_addr = (const char*)&val.b;
    const char* c_addr = ! val.c_null ? (const char*)val.c : 0;
    const char* d_addr = ! val.d_null ? (const char*)&val.d : 0;
    Uint32 no = 0;
    chkdb(g_op->equal(no++, a_addr) == 0);
    chkdb(g_op->setValue(no++, b_addr) == 0);
    chkdb(g_op->setValue(no++, c_addr) == 0);
    chkdb(g_op->setValue(no++, d_addr) == 0);
    if (i++ % batch == 0) {
      chkdb(g_con->execute(NdbTransaction::Commit) == 0);
      g_ndb->closeTransaction(g_con);
      g_con = 0;
      g_op = 0;
      chkdb((g_con = g_ndb->startTransaction()) != 0);
    }
  }
  chkdb(g_con->execute(NdbTransaction::Commit) == 0);
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_op = 0;
  ll0(g_tabname << ": inserted " << g_opts.rows << " rows");
  return 0;
}

static int
countrows()
{
  Uint64 rows = 0;
  Uint64 r;
  char* r_addr = (char*)&r;
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  chkdb((g_scan_op = g_con->getNdbScanOperation(g_tab)) != 0);
  chkdb(g_scan_op->readTuples() == 0);
  chkdb(g_scan_op->interpret_exit_last_row() == 0);
  chkdb(g_scan_op->getValue(NdbDictionary::Column::ROW_COUNT, r_addr) != 0);
  chkdb(g_con->execute(NdbTransaction::NoCommit) == 0);
  while (1) {
    int ret;
    r = ~(Uint64)0;
    chkdb((ret = g_scan_op->nextResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    rows += r;
  }
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_scan_op = 0;
  g_opts.rows = rows;
  return 0;
}

static int
scandata()
{
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  chkdb((g_scan_op = g_con->getNdbScanOperation(g_tab)) != 0);
  chkdb(g_scan_op->readTuples() == 0);
  Uint32 a;
  Val val;
  char* a_addr = (char*)&a;
  char* b_addr = (char*)&val.b;
  char* c_addr = (char*)val.c;
  char* d_addr = (char*)&val.d;
  Uint32 no = 0;
  NdbRecAttr* b_ra;
  NdbRecAttr* c_ra;
  NdbRecAttr* d_ra;
  chkdb(g_scan_op->getValue(no++, a_addr) != 0);
  chkdb((b_ra = g_scan_op->getValue(no++, b_addr)) != 0);
  chkdb((c_ra = g_scan_op->getValue(no++, c_addr)) != 0);
  chkdb((d_ra = g_scan_op->getValue(no++, d_addr)) != 0);
  chkdb(g_con->execute(NdbTransaction::NoCommit) == 0);
  uint count = 0;
  uint i;
  for (i = 0; i < g_opts.rows; i++)
    g_keys[i].count = 0;
  while (1) {
    int ret;
    a = ~(Uint32)0;
    chkdb((ret = g_scan_op->nextResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    assert(b_ra->isNULL() == 0 && c_ra->isNULL() != -1 && d_ra->isNULL() != -1);
    val.c_null = c_ra->isNULL();
    val.d_null = d_ra->isNULL();
    i = (uint)a;
    chkrc(i < g_opts.rows);
    Key& key = g_keys[i];
    if (g_opts.loaddata)
      chkrc(key.val.cmp(val) == 0);
    else
      key.val = val;
    key.count++;
    count++;
  }
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_scan_op = 0;
  for (i = 0; i < g_opts.rows; i++)
    chkrc(g_keys[i].count == 1);
  assert(count == g_opts.rows);
  int level = g_opts.loaddata ? 1 : 0;
  llx(level, g_tabname << ": scanned " << g_opts.rows << " rows");
  return 0;
}

static int
loaddata()
{
  if (g_opts.loaddata) {
    chkrc(allockeys() == 0);
    makekeys();
    chkrc(insertdata() == 0);
  } else {
    chkrc(countrows() == 0);
    chkrc(g_opts.rows != 0);
    ll0(g_tabname << ": using old table of " << g_opts.rows << " rows");
    chkrc(allockeys() == 0);
  }
  chkrc(scandata() == 0);
  uint i;
  for (i = 0; i < g_opts.rows; i++)
    ll3(i << ": " << g_keys[i]);
  return 0;
}

// true = match, index = match or next higher
static bool
sortval(const Val& val, int& index)
{
  if (unlikely(g_sortcount == 0)) {
    index = 0;
    return false;
  }
  int lo = -1;
  int hi = (int)g_sortcount;
  int ret;
  int j;
  do {
    j = (hi + lo) / 2;
    ret = val.cmp(g_sortkeys[j].val);
    if (ret < 0)
      hi = j;
    else if (ret > 0)
      lo = j;
    else
      break;
  } while (hi - lo > 1);
  if (ret == 0) {
    index = j;
    return true;
  }
  index = hi;
  return false;
}

static void
sortkeys()
{
  // insert sort with binary search
  g_sortcount = 0;
  uint i;
  for (i = 0; i < g_opts.rows; i++) {
    const Val& val = g_keys[i].val;
    int index;
    bool match = sortval(val, index);
    Key& dst = g_sortkeys[index];
    if (match) {
      dst.rpk++;
    } else {
      uint bytes = ((int)g_sortcount - index) * sizeof(Key);
      memmove(&dst + 1, &dst, bytes);
      dst.val = val;
      dst.rpk = 1;
      g_sortcount++;
    }
  }
  g_minkey = &g_sortkeys[0];
  g_maxkey = &g_sortkeys[g_sortcount - 1];
  ll1("counted " << g_sortcount << " distinct keys");
}

struct Bnd {
  Val val;
  /*
   * A bound is a partial key value (0 to g_numattrs attributes).
   * It is not equal to any key value.  Instead, it has a "side".
   *
   * side = 0 if the bound is empty
   * side = -1 if the bound is "just before" its value
   * side = +1 if the bound is "just after" its value
   *
   * This is another way of looking at strictness of non-empty
   * start and end keys in a range.
   *
   * start key is strict if side = +1
   * end key is strict if side = -1
   *
   * NDB API specifies strictness in the bound type of the last
   * index attribute which is part of the start/end key.
   *
   * LE (0) - strict: n - side: -1
   * LT (1) - strict: y - side: +1
   * GE (2) - strict: n - side: +1
   * GT (3) - strict: y - side: -1
   *
   * A non-empty bound divides keys into 2 disjoint subsets:
   * keys before (cmp() == -1) and keys after (cmp() == +1).
   */
  int side;
  Bnd& make(uint minattrs);
  Bnd& make(uint minattrs, const Val& theval);
  int cmp(const Val& val) const;
  int type(uint lohi, uint colno) const; // for setBound
};

static NdbOut&
operator<<(NdbOut& out, const Bnd& bnd)
{
  out << bnd.val;
  out << " side: " << bnd.side;
  return out;
}

Bnd&
Bnd::make(uint minattrs)
{
  uint numattrs = minattrs + urandom(g_numattrs - minattrs);
  val.make(numattrs, false);
  side = val.numattrs == 0 ? 0 : urandom(2) == 0 ? -1 : +1;
  return *this;
}

Bnd&
Bnd::make(uint minattrs, const Val& theval)
{
  uint numattrs = minattrs + urandom(g_numattrs - minattrs);
  val = theval;
  val.numattrs = numattrs;
  side = val.numattrs == 0 ? 0 : urandom(2) == 0 ? -1 : +1;
  return *this;
}

int
Bnd::cmp(const Val& theval) const
{
  int place; // debug
  int ret;
  do {
    assert(theval.numattrs == g_numattrs);
    int k = theval.cmp(val, val.numattrs);
    if (k != 0) {
      place = 1;
      ret = k;
      break;
    }
    if (side != 0) {
      place = 2;
      ret = -side;
      break;
    }
    place = 3;
    ret = 0;
    assert(val.numattrs == 0);
  } while (0);
  ll3("cmp: val: " << theval << " bnd: " << *this <<
      " return: " << ret << " at " << place);
  return ret;
}

int
Bnd::type(uint lohi, uint colno) const
{
  int t;
  assert(lohi <= 1 && colno < val.numattrs && (side == -1 || side == +1));
  if (lohi == 0) {
    if (colno + 1 < val.numattrs)
      t = 0; // LE
    else if (side == -1)
      t = 0; // LE
    else
      t = 1; // LT
  } else {
    if (colno + 1 < val.numattrs)
      t = 2; // GE
    else if (side == +1)
      t = 2; // GE
    else
      t = 3; // GT
  }
  return t;
}

struct Range {
  Bnd bnd[2];
  uint minattrs() const;
  uint maxattrs() const;
  int cmp(const Val& val) const; // -1,0,+1 = key is before,in,after range
  uint rowcount() const;
  bool iseq() const;
  // stats
  bool flag;
  uint statrows;
  uint scanrows;
  double errpct;
};

static NdbOut&
operator<<(NdbOut& out, const Range& range)
{
  out << "bnd0: " << range.bnd[0] << " bnd1: " << range.bnd[1];
  return out;
}

uint
Range::minattrs() const
{
  return min(bnd[0].val.numattrs, bnd[1].val.numattrs);
}

uint
Range::maxattrs() const
{
  return max(bnd[0].val.numattrs, bnd[1].val.numattrs);
}

int
Range::cmp(const Val& theval) const
{
  int place; // debug
  int ret;
  do  {
    int k;
    k = bnd[0].cmp(theval);
    if (k < 0) {
      place = 1;
      ret = -1;
      break;
    }
    k = bnd[1].cmp(theval);
    if (k > 0) {
      place = 2;
      ret = +1;
      break;
    }
    place = 3;
    ret = 0;
  } while (0);
  ll3("cmp: val: " << theval << " range: " << *this <<
      " return: " << ret << " at " << place);
  return ret;
}

uint
Range::rowcount() const
{
  ll2("rowcount: " << *this);
  int i;
  // binary search for first and last in range
  int lim[2];
  for (i = 0; i <= 1; i++) {
    ll3("search i=" << i);
    int lo = -1;
    int hi = (int)g_sortcount;
    int ret;
    int j;
    do {
      j = (hi + lo) / 2;
      ret = cmp(g_sortkeys[j].val);
      if (i == 0) {
        if (ret < 0)
          lo = j;
        else
          hi = j;
      } else {
        if (ret > 0)
          hi = j;
        else
          lo = j;
      }
    } while (hi - lo > 1);
    if (ret == 0)
      lim[i] = j;
    else if (i == 0)
      lim[i] = hi;
    else
      lim[i] = lo;
  }
  // the range
  const int lo = max(lim[0], 0);
  const int hi = min(lim[1], (int)g_sortcount - 1);
  if (! g_opts.nochecks) {
    int curr = -1;
    for (i = 0; i < (int)g_sortcount; i++) {
      int k = cmp(g_sortkeys[i].val);
      if (k < 0)
        assert(i < lo);
      else if (k == 0)
        assert(lo <= i && i <= hi);
      else
        assert(i > hi);
      assert(curr <= k);
      if (curr < k)
        curr = k;
    }
  }
  // sum them up
  uint count = 0;
  for (i = lo; i <= hi; i++)
    count += g_sortkeys[i].count;
  ll2("count: " << count << " index lim: " << lim[0] << " " << lim[1]);
  return count;
}

bool
Range::iseq() const
{
  return
    minattrs() == maxattrs() &&
    bnd[0].val.cmp(bnd[1].val, minattrs()) == 0 &&
    bnd[0].side < bnd[1].side;
}

static Range* g_ranges = 0;

static void
freeranges()
{
  if (g_ranges != 0)
    my_free(g_ranges);
  g_ranges = 0;
}

static int
allocranges()
{
  freeranges();
  size_t sz = sizeof(Range) * g_opts.ops;
  g_ranges = (Range*)my_malloc(sz, MYF(0));
  chkrc(g_ranges != 0);
  memset(g_ranges, 0x1f, sz);
  return 0;
}

static void
makeranges()
{
  uint i;
  for (i = 0; i < g_opts.ops; i++) {
    Range& range = g_ranges[i];
    range.flag = false; // mark for dup generation done
    bool fulleq = (urandom(100) < g_opts.eqscans);
    bool eq = fulleq || (urandom(100) < g_opts.eqscans);
    bool matcheq = eq && (urandom(10) != 0);
    if (! eq) {
      // random but prefer non-empty and no more than scanpct
      do {
        range.bnd[0].make(0);
        range.bnd[1].make(0);
        uint count = range.rowcount();
        if (count != 0 && 100 * count <= g_opts.scanpct * g_opts.rows)
          break;
      } while (urandom(g_opts.tryhard) != 0);
    } else {
      uint minattrs = fulleq ? g_numattrs : 1;
      if (! matcheq) {
        range.bnd[0].make(minattrs);
      } else {
        uint m = urandom(g_sortcount);
        const Val& val = g_sortkeys[m].val;
        range.bnd[0].make(minattrs, val);
      }
      range.bnd[1] = range.bnd[0];
      range.bnd[0].side = -1;
      range.bnd[1].side = +1;
      // fix types
      range.bnd[0];
      range.bnd[1];
      assert(range.iseq());
    }
  }
  for (i = 0; i < g_opts.ops; i++) {
    Range& range = g_ranges[i];
    if (range.flag)
      continue;
    range.flag = true;
    if (urandom(100) < g_opts.dupscans) {
      uint j = urandom(g_opts.ops);
      do {
        Range& dst = g_ranges[j];
        if (! dst.flag) {
          dst.bnd[0] = range.bnd[0];
          dst.bnd[1] = range.bnd[1];
          dst.flag = true;
          break;
        }
      } while (urandom(g_opts.tryhard) != 0);
    }
  }
}

static int
setbounds(const Range& range)
{
  // currently must do each attr in order
  ll2("setbounds: " << range);
  uint i;
  const Bnd (&bnd)[2] = range.bnd;
  for (i = 0; i < g_numattrs; i++) {
    const Uint32 no = i; // index attribute number
    uint j;
    int type[2] = { -1, -1 };
    for (j = 0; j <= 1; j++) {
      if (no < bnd[j].val.numattrs)
        type[j] = bnd[j].type(j, no);
    }
    for (j = 0; j <= 1; j++) {
      int t = type[j];
      if (t == -1)
        continue;
      if (no + 1 < bnd[j].val.numattrs)
        t &= ~(uint)1; // strict bit is set on last bound only
      const Val& val = bnd[j].val;
      const void* addr = 0;
      if (no == 0)
        addr = (const void*)&val.b;
      else if (no == 1)
        addr = ! val.c_null ? (const void*)val.c : 0;
      else if (no == 2)
        addr = ! val.d_null ? (const void*)&val.d : 0;
      else
        assert(false);
      ll2("setBound attr:" << no << " type:" << t << " val: " << val);
      chkdb(g_rangescan_op->setBound(no, t, addr) == 0);
    }
  }
  return 0;
}

static int
allocstat()
{
  g_stat = new NdbIndexStat(g_ind);
  chkdb(g_stat->alloc_cache(32) == 0);
  return 0;
}

static int
runstat(Range& range, int flags)
{
  ll2("runstat: " << range << " flags=" << flags);
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  chkdb((g_rangescan_op = g_con->getNdbIndexScanOperation(g_ind, g_tab)) != 0);
  chkdb(g_rangescan_op->readTuples(NdbOperation::LM_CommittedRead) == 0);
  chkrc(setbounds(range) == 0);
  Uint64 count = ~(Uint64)0;
  chkdb(g_stat->records_in_range(g_ind, g_rangescan_op, g_opts.rows, &count, flags) == 0);
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_rangescan_op = 0;
  range.statrows = (uint)count;
  chkrc((Uint64)range.statrows == count);
  ll2("stat: " << range.statrows);
  return 0;
}

static int
runscan(Range& range)
{
  ll2("runscan: " << range);
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  chkdb((g_rangescan_op = g_con->getNdbIndexScanOperation(g_ind, g_tab)) != 0);
  chkdb(g_rangescan_op->readTuples() == 0);
  chkrc(setbounds(range) == 0);
  Uint32 a;
  char* a_addr = (char*)&a;
  Uint32 no = 0;
  chkdb(g_rangescan_op->getValue(no++, a_addr) != 0);
  chkdb(g_con->execute(NdbTransaction::NoCommit) == 0);
  uint count = 0;
  uint i;
  for (i = 0; i < g_opts.rows; i++)
    g_keys[i].count = 0;
  while (1) {
    int ret;
    a = ~(Uint32)0;
    chkdb((ret = g_rangescan_op->nextResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    i = (uint)a;
    chkrc(i < g_opts.rows);
    Key& key = g_keys[i];
    ll3("scan: " << key);
    int k = range.cmp(key.val);
    chkrc(k == 0);
    chkrc(key.count == 0);
    key.count++;
    count++;
  }
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_rangescan_op = 0;
  if (! g_opts.nochecks) {
    for (i = 0; i < g_opts.rows; i++) {
      const Key& key = g_keys[i];
      int k = range.cmp(key.val);
      assert((k != 0 && key.count == 0) || (k == 0 && key.count == 1));
    }
    assert(range.rowcount() == count);
  }
  range.scanrows = count;
  ll2("scan: " << range.scanrows);
  return 0;
}

static int
runscans()
{
  uint i;
  for (i = 0; i < g_opts.ops; i++) {
    Range& range = g_ranges[i];
    ll1("range " << i << ": " << range);
    // simulate old handler code
    int flags = 0;
    if (i < 32 || i % 20 == 0)
      flags |= NdbIndexStat::RR_UseDb;
    chkrc(runstat(range, flags) == 0);
    chkrc(runscan(range) == 0);
    // if stat is 0 then it is exact scan count
    chkrc(range.statrows != 0 || range.scanrows == 0);
    // measure error as fraction of total rows
    double x = (double)range.statrows;
    double y = (double)range.scanrows;
    double z = (double)g_opts.rows;
    double err = (x - y) / z;
    // report in pct
    range.errpct = 100.0 * err;
    ll1("range " << i << ":" <<
        " stat: " << range.statrows << " scan: " << range.scanrows <<
        " errpct: " << range.errpct);
  }
  return 0;
}

struct Stat {
  const char* name;
  uint count;
  double sum;
  double minval;
  double maxval;
  double avg;
  double varsum;
  double var;
  double stddev;
  void init();
  void add(const Stat& stat);
};

void 
Stat::init()
{
  name = "stat";
  count = 0;
  sum = minval = maxval = avg = varsum = var = stddev = 0.0;
}

void
Stat::add(const Stat& stat)
{
  if (count == 0) {
    *this = stat;
    return;
  }
  Stat tmp = *this;
  tmp.count = count + stat.count;
  tmp.sum = sum + stat.sum;
  tmp.minval = minval <= stat.minval ? minval : stat.minval;
  tmp.maxval = maxval >= stat.maxval ? maxval : stat.maxval;
  tmp.avg = tmp.sum / double(tmp.count);
  tmp.varsum = varsum + stat.varsum;
  tmp.var = tmp.varsum / double(tmp.count);
  tmp.stddev = sqrt(tmp.var);
  *this = tmp;
}

static NdbOut&
operator<<(NdbOut& out, const Stat& stat)
{
  out << stat.name << ": " << "count: " << stat.count
      << " min: " << stat.minval << " max: " << stat.maxval
      << " avg: " << stat.avg << " stddev: " << stat.stddev;
  return out;
}

template <class T, class V>
static void
computestat(Stat& stat)
{
  stat.init();
  stat.name = V::name();
  const T* array = V::array();
  stat.count = V::count();
  assert(stat.count != 0);
  uint i;
  for (i = 0; i < stat.count; i++) {
    const T& item = array[i];
    double data = V::data(item);
    stat.sum += data;
    if (i == 0)
      stat.minval = stat.maxval = data;
    else {
      if (stat.minval > data)
        stat.minval = data;
      if (stat.maxval < data)
        stat.maxval = data;
    }
  }
  stat.avg = stat.sum / double(stat.count);
  stat.varsum = 0.0;
  for (i = 0; i < stat.count; i++) {
    const T& item = array[i];
    double data = V::data(item);
    double x = data - stat.avg;
    stat.varsum += x * x;
  }
  stat.var = stat.varsum / double(stat.count);
  stat.stddev = sqrt(stat.var);
}

struct V_rpk {
  static const char* name() { return "rec per key"; }
  static const Key* array() { return g_sortkeys; }
  static uint count() { return g_sortcount; }
  static double data(const Key& key) { return (double)key.rpk; }
};

struct V_rir {
  static const char* name() { return "rir err pct"; }
  static const Range* array() { return g_ranges; }
  static uint count() { return g_opts.ops; }
  static double data(const Range& range) { return (double)range.errpct; }
};

template void computestat<Key, V_rpk>(Stat& stat);
template void computestat<Range, V_rir>(Stat& stat);

static Stat g_stat_rpk; // summaries over loops
static Stat g_stat_rir;

static void
loopstats()
{
  Stat stat_rpk; // records per key
  Stat stat_rir; // record in range
  if (g_loop == 0) {
    g_stat_rpk.init();
    g_stat_rir.init();
  }
  computestat<Key, V_rpk>(stat_rpk);
  computestat<Range, V_rir>(stat_rir);
  if (g_opts.loop != 1) {
    ll0("=== loop " << g_loop << " summary ===");
    ll0(stat_rpk);
    ll0(stat_rir);
  }
  // accumulate
  g_stat_rpk.add(stat_rpk);
  g_stat_rir.add(stat_rir);
}

static void
finalstats()
{
  ll0("=== summary ===");
  ll0(g_stat_rpk);
  ll0(g_stat_rir);
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
  g_cs = get_charset_by_name(g_csname, MYF(0));
  if (g_cs == 0)
    g_cs = get_charset_by_csname(g_csname, MY_CS_PRIMARY, MYF(0));
  chkrc(g_cs != 0);
  for (g_loop = 0; g_opts.loop == 0 || g_loop < g_opts.loop; g_loop++) {
    ll0("=== loop " << g_loop << " ===");
    setseed(g_loop);
    chkrc(createtable() == 0);
    chkrc(loaddata() == 0);
    sortkeys();
    chkrc(allocranges() == 0);
    makeranges();
    chkrc(allocstat() == 0);
    chkrc(runscans() == 0);
    chkrc(droptable() == 0);
    loopstats();
  }
  finalstats();
  return 0;
}

NDB_STD_OPTS_VARS;

static struct my_option
my_long_options[] =
{
  NDB_STD_OPTS("testIndexStat"),
  { "loglevel", 1001, "Logging level in this program 0-3 (default 0)",
    &g_opts.loglevel, &g_opts.loglevel, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "seed", 1002, "Random seed (0=loop number, default -1=random)",
    &g_opts.seed, &g_opts.seed, 0,
    GET_INT, REQUIRED_ARG, -1, 0, 0, 0, 0, 0 },
  { "loop", 1003, "Number of test loops (default 1, 0=forever)",
    &g_opts.loop, &g_opts.loop, 0,
    GET_INT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0 },
  { "rows", 1004, "Number of rows (default 100000)",
    &g_opts.rows, &g_opts.rows, 0,
    GET_UINT, REQUIRED_ARG, 100000, 0, 0, 0, 0, 0 },
  { "ops", 1005, "Number of index scans per loop (default 1000)",
    &g_opts.ops, &g_opts.ops, 0,
    GET_UINT, REQUIRED_ARG, 1000, 0, 0, 0, 0, 0 },
  { "dupkeys", 1006, "Pct records per key (min 100, default 1000)",
    &g_opts.dupkeys, &g_opts.dupkeys, 0,
    GET_UINT, REQUIRED_ARG, 1000, 0, 0, 0, 0, 0 },
  { "scanpct", 1007, "Preferred max pct of total rows per scan (default 5)",
    &g_opts.scanpct, &g_opts.scanpct, 0,
    GET_UINT, REQUIRED_ARG, 5, 0, 0, 0, 0, 0 },
  { "nullkeys", 1008, "Pct nulls in each key attribute (default 10)",
    &g_opts.nullkeys, &g_opts.nullkeys, 0,
    GET_UINT, REQUIRED_ARG, 10, 0, 0, 0, 0, 0 },
  { "eqscans", 1009, "Pct scans for partial/full equality (default 50)",
    &g_opts.eqscans, &g_opts.eqscans, 0,
    GET_UINT, REQUIRED_ARG, 50, 0, 0, 0, 0, 0 },
  { "dupscans", 1010, "Pct scans using same bounds (default 10)",
    &g_opts.dupscans, &g_opts.dupscans, 0,
    GET_UINT, REQUIRED_ARG, 10, 0, 0, 0, 0, 0 },
  { "keeptable", 1011, "Use existing table and data if any and do not drop",
    &g_opts.keeptable, &g_opts.keeptable, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "no-extra-checks", 1012, "Omit expensive consistency checks",
    &g_opts.nochecks, &g_opts.nochecks, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "abort-on-error", 1013, "Dump core on any error",
    &g_opts.abort, &g_opts.abort, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0,
    0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

static void
usage()
{
  ndbout
    << g_progname
    << ": measure records_in_range error as percentage of total rows" << endl;
  my_print_help(my_long_options);
}

static int
checkoptions()
{
  chkrc(g_opts.rows != 0);
  chkrc(g_opts.nullkeys <= 100);
  chkrc(g_opts.dupkeys >= 100);
  chkrc(g_opts.scanpct <= 100);
  chkrc(g_opts.eqscans <= 100);
  chkrc(g_opts.dupscans <= 100);
  return 0;
}

static int
doconnect()
{
  g_ncc = new Ndb_cluster_connection();
  chkdb(g_ncc->connect(30) == 0);
  g_ndb = new Ndb(g_ncc, "TEST_DB");
  chkdb(g_ndb->init() == 0 && g_ndb->waitUntilReady(30) == 0);
  return 0;
}

static void
freeall()
{
  delete g_stat;
  freekeys();
  freeranges();
  delete g_ndb;
  delete g_ncc;
}

int
main(int argc, char** argv)
{
  ndb_init();
  const char* g_progname =
    strchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
  uint i;
  ndbout << g_progname;
  for (i = 1; i < argc; i++)
    ndbout << " " << argv[i];
  ndbout << endl;
  int ret;
  ret = handle_options(&argc, &argv, my_long_options, ndb_std_get_one_option);
  if (ret != 0 || argc != 0)
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  if (checkoptions() == 0 && doconnect() == 0 && runtest() == 0) {
    freeall();
    return NDBT_ProgramExit(NDBT_OK);
  }
  freeall();
  return NDBT_ProgramExit(NDBT_FAILED);
}
