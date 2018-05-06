/*
   Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NdbApi.hpp>
#include <NdbIndexStat.hpp>
#include <NdbTest.hpp>
#include <ndb_version.h>
#include <NDBT_Stats.hpp>
#include <math.h>
#include <NdbHost.h>

#undef min
#undef max
#define min(a, b) ((a) <= (b) ? (a) : (b))
#define max(a, b) ((a) >= (b) ? (a) : (b))

struct Opts {
  int loglevel;
  uint seed;
  uint attrs;
  uint loops;
  uint rows;
  uint ops;
  uint nullkeys;
  uint rpk;
  uint rpkvar;
  uint scanpct;
  uint eqscans;
  bool keeptable;
  bool abort;
  const char* dump;
  Opts() :
    loglevel(0),
    seed(0),
    attrs(3),
    loops(1),
    rows(10000),
    ops(100),
    nullkeys(10),
    rpk(10),
    rpkvar(10),
    scanpct(10),
    eqscans(30),
    keeptable(false),
    abort(false),
    dump(0)
  {}
};

static Opts g_opts;
static uint g_loop = 0;

static const char* g_tabname = "ts1";
static const char* g_indname = "ts1x1";
static const uint g_numattrs = 3;
static const uint g_charlen = 10;
static const char* g_csname = "latin1_swedish_ci";
static CHARSET_INFO* g_cs;

// keys nullability
static const bool g_b_nullable = true;
static const bool g_c_nullable = true;
static const bool g_d_nullable = true;

// value limits
struct Lim {
  bool all_nullable;
  uint b_min;
  uint b_max;
  const char* c_char;
  uint d_min;
  uint d_max;
};

static Lim g_lim_val;
static Lim g_lim_bnd;

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static Ndb* g_ndb_sys = 0;
static NdbDictionary::Dictionary* g_dic = 0;
static const NdbDictionary::Table* g_tab = 0;
static const NdbDictionary::Index* g_ind = 0;
static const NdbRecord* g_tab_rec = 0;
static const NdbRecord* g_ind_rec = 0;

struct my_record
{
  Uint8 m_null_bm;
  Uint8 fill[3];
  Uint32 m_a;
  Uint32 m_b;
  char m_c[1+g_charlen];
  Uint16 m_d;
};

static const Uint32 g_ndbrec_a_offset=offsetof(my_record, m_a);
static const Uint32 g_ndbrec_b_offset=offsetof(my_record, m_b);
static const Uint32 g_ndbrec_b_nb_offset=1;
static const Uint32 g_ndbrec_c_offset=offsetof(my_record, m_c);
static const Uint32 g_ndbrec_c_nb_offset=2;
static const Uint32 g_ndbrec_d_offset=offsetof(my_record, m_d);
static const Uint32 g_ndbrec_d_nb_offset=3;

static NdbTransaction* g_con = 0;
static NdbOperation* g_op = 0;
static NdbScanOperation* g_scan_op = 0;
static NdbIndexScanOperation* g_rangescan_op = 0;

static NdbIndexStat* g_is = 0;
static bool g_has_created_stat_tables = false;
static bool g_has_created_stat_events = false;

static uint
urandom(uint m)
{
  if (m == 0)
    return 0;
  uint r = (uint)rand();
  r = r % m;
  return r;
}

static int& g_loglevel = g_opts.loglevel; // default log level

#define chkdb(x) \
  do { if (likely(x)) break; ndbout << "line " << __LINE__ << " FAIL " << #x << endl; errdb(); if (g_opts.abort) abort(); return -1; } while (0)

#define chker(x) \
  do { if (likely(x)) break; ndbout << "line " << __LINE__ << " FAIL " << #x << endl; ndbout << "errno: " << errno; if (g_opts.abort) abort(); return -1; } while (0)

#define chkrc(x) \
  do { if (likely(x)) break; ndbout << "line " << __LINE__ << " FAIL " << #x << endl; if (g_opts.abort) abort(); return -1; } while (0)

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
  if (g_ncc != 0) {
    NdbError e;
    e.code = g_ncc->get_latest_error();
    e.message = g_ncc->get_latest_error_msg();
    if (e.code != 0)
      ll0(++any << " ncc: error" << e);
  }
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
  if (g_is != 0) {
    const NdbIndexStat::Error& e = g_is->getNdbError();
    if (e.code != 0)
      ll0(++any << " stat: error " << e);
  }
  if (! any)
    ll0("unknown db error");
}

/* Methods to create NdbRecord structs for the table and index */
static int
createNdbRecords()
{
  ll1("createNdbRecords");
  const Uint32 numCols=4;
  const Uint32 numIndexCols=3;
  NdbDictionary::RecordSpecification recSpec[numCols];

  recSpec[0].column= g_tab->getColumn("a"); // 4 bytes
  recSpec[0].offset= g_ndbrec_a_offset;
  recSpec[0].nullbit_byte_offset= ~(Uint32)0;
  recSpec[0].nullbit_bit_in_byte= ~(Uint32)0;
 
  recSpec[1].column= g_tab->getColumn("b"); // 4 bytes
  recSpec[1].offset= g_ndbrec_b_offset;
  if (g_b_nullable) {
    recSpec[1].nullbit_byte_offset= 0;
    recSpec[1].nullbit_bit_in_byte= g_ndbrec_b_nb_offset;
  } else {
    recSpec[1].nullbit_byte_offset= ~(Uint32)0;
    recSpec[1].nullbit_bit_in_byte= ~(Uint32)0;
  }
 
  recSpec[2].column= g_tab->getColumn("c"); // Varchar(10) -> ~12 bytes
  recSpec[2].offset= g_ndbrec_c_offset;
  if (g_c_nullable) {
    recSpec[2].nullbit_byte_offset= 0;
    recSpec[2].nullbit_bit_in_byte= g_ndbrec_c_nb_offset;
  } else {
    recSpec[2].nullbit_byte_offset= ~(Uint32)0;
    recSpec[2].nullbit_bit_in_byte= ~(Uint32)0;
  }

  recSpec[3].column= g_tab->getColumn("d"); // 2 bytes
  recSpec[3].offset= g_ndbrec_d_offset;
  if (g_d_nullable) {
    recSpec[3].nullbit_byte_offset= 0;
    recSpec[3].nullbit_bit_in_byte= g_ndbrec_d_nb_offset;
  } else {
    recSpec[3].nullbit_byte_offset= ~(Uint32)0;
    recSpec[3].nullbit_bit_in_byte= ~(Uint32)0;
  }

  g_dic = g_ndb->getDictionary();
  g_tab_rec= g_dic->createRecord(g_tab,
                                 &recSpec[0],
                                 numCols,
                                 sizeof(NdbDictionary::RecordSpecification),
                                 0);

  chkdb(g_tab_rec != NULL);

  g_ind_rec= g_dic->createRecord(g_ind,
                                 &recSpec[1],
                                 numIndexCols,
                                 sizeof(NdbDictionary::RecordSpecification),
                                 0);
  
  chkdb(g_ind_rec != NULL);
  g_dic = 0;

  return 0;
}

// create table ts0 (
//   a int unsigned,
//   b int unsigned, c varchar(10), d smallint unsigned,
//   primary key using hash (a), index (b, c, d) )

static int
createtable()
{
  ll1("createtable");
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
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(g_b_nullable);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("c");
    col.setType(NdbDictionary::Column::Varchar);
    col.setLength(g_charlen);
    col.setCharset(g_cs);
    col.setNullable(g_c_nullable);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("d");
    col.setType(NdbDictionary::Column::Smallunsigned);
    col.setNullable(g_d_nullable);
    tab.addColumn(col);
  }

  g_dic = g_ndb->getDictionary();
  if (g_dic->getTable(g_tabname) != 0)
    chkdb(g_dic->dropTable(g_tabname) == 0);
  chkdb(g_dic->createTable(tab) == 0);
  chkdb((g_tab = g_dic->getTable(g_tabname)) != 0);
  g_dic = 0;
  return 0;
}

static int
createindex()
{
  ll1("createindex");
  NdbDictionary::Index ind(g_indname);
  ind.setTable(g_tabname);
  ind.setType(NdbDictionary::Index::OrderedIndex);
  ind.setLogging(false);
  ind.addColumnName("b");
  ind.addColumnName("c");
  ind.addColumnName("d");

  g_dic = g_ndb->getDictionary();
  chkdb(g_dic->createIndex(ind) == 0);
  chkdb((g_ind = g_dic->getIndex(g_indname, g_tabname)) != 0);
  g_dic = 0;
  return 0;
}

static int
droptable()
{
  ll1("droptable");
  g_dic = g_ndb->getDictionary();
  chkdb(g_dic->dropTable(g_tabname) == 0);
  g_dic = 0;
  return 0;
}

// values for keys and bounds

struct Val {
  uint8 m_numattrs;
  int8 b_null;
  int8 c_null;
  int8 d_null;
  Uint32 b;
  uchar c[1 + g_charlen];
  Uint16 d;
  Val();
  void init();
  void copy(const Val& val2);
  void make(uint numattrs, const Lim& lim);
  int cmp(const Val& val2, uint numattrs = g_numattrs, uint* num_eq = 0) const;
  void fromib(const NdbIndexScanOperation::IndexBound& ib, uint j);

private:
  Val& operator=(const Val&);
  Val(const Val&);
};

static NdbOut&
operator<<(NdbOut& out, const Val& val)
{
  out << "[";
  if (val.m_numattrs >= 1) {
    if (val.b_null)
      out << "NULL";
    else
      out << val.b;
  }
  if (val.m_numattrs >= 2) {
    out << " ";
    if (val.c_null)
      out << "NULL";
    else {
      char buf[1 + g_charlen];
      sprintf(buf, "%.*s", val.c[0], &val.c[1]);
      out << "'" << buf << "'";
    }
  }
  if (val.m_numattrs >= 3) {
    out << " ";
    if (val.d_null)
      out <<" NULL";
    else
      out << val.d;
  }
  out << "]";
  return out;
}

Val::Val()
{
  init();
}

void
Val::init()
{
  m_numattrs = 0;
  // junk rest
  b_null = -1;
  c_null = -1;
  d_null = -1;
  b = ~(Uint32)0;
  memset(c, 0xff, sizeof(c));
  d = ~(Uint16)0;
}

void
Val::copy(const Val& val2)
{
  require(this != &val2);
  init();
  m_numattrs = val2.m_numattrs;
  if (m_numattrs >= 1) {
    require(val2.b_null == 0 || val2.b_null == 1);
    b_null = val2.b_null;
    if (!b_null)
      b = val2.b;
  }
  if (m_numattrs >= 2) {
    require(val2.c_null == 0 || val2.c_null == 1);
    c_null = val2.c_null;
    if (!c_null)
      memcpy(c, val2.c, sizeof(c));
  }
  if (m_numattrs >= 3) {
    require(val2.d_null == 0 || val2.d_null == 1);
    d_null = val2.d_null;
    if (!d_null)
      d = val2.d;
  }
}

void
Val::make(uint numattrs, const Lim& lim)
{
  require(numattrs <= g_numattrs);
  if (numattrs >= 1) {
    const bool nullable = g_b_nullable || lim.all_nullable;
    if (nullable && urandom(100) < g_opts.nullkeys)
      b_null = 1;
    else {
      require(lim.b_min <= lim.b_max);
      b = lim.b_min + urandom(lim.b_max - lim.b_min + 1);
      b_null = 0;
    }
  }
  if (numattrs >= 2) {
    const bool nullable = g_c_nullable || lim.all_nullable;
    if (nullable && urandom(100) < g_opts.nullkeys)
      c_null = 1;
    else {
      // prefer shorter
      const uint len = urandom(urandom(g_charlen + 1) + 1);
      c[0] = len;
      for (uint j = 0; j < len; j++) {
        uint k = urandom((uint)strlen(lim.c_char));
        c[1 + j] = lim.c_char[k];
      }
      c_null = 0;
    }
  }
  if (numattrs >= 3) {
    const bool nullable = g_d_nullable || lim.all_nullable;
    if (nullable && urandom(100) < g_opts.nullkeys)
      d_null = 1;
    else {
      require(lim.d_min <= lim.d_max);
      d = lim.d_min + urandom(lim.d_max - lim.d_min + 1);
      d_null = 0;
    }
  }
  m_numattrs = numattrs;
}

int
Val::cmp(const Val& val2, uint numattrs, uint* num_eq) const
{
  require(numattrs <= m_numattrs);
  require(numattrs <= val2.m_numattrs);
  uint n = 0; // attr index where differs
  uint k = 0;
  if (k == 0 && numattrs >= 1) {
    if (! b_null && ! val2.b_null) {
      if (b < val2.b)
        k = -1;
      else if (b > val2.b)
        k = +1;
    } else if (! b_null) {
      k = +1;
    } else if (! val2.b_null) {
      k = -1;
    }
    if (k == 0)
      n++;
  }
  if (k == 0 && numattrs >= 2) {
    if (! c_null && ! val2.c_null) {
      const uchar* s1 = &c[1];
      const uchar* s2 = &val2.c[1];
      const uint l1 = (uint)c[0];
      const uint l2 = (uint)val2.c[0];
      require(l1 <= g_charlen && l2 <= g_charlen);
      k = g_cs->coll->strnncollsp(g_cs, s1, l1, s2, l2);
    } else if (! c_null) {
      k = +1;
    } else if (! val2.c_null) {
      k = -1;
    }
    if (k == 0)
      n++;
  }
  if (k == 0 && numattrs >= 3) {
    if (! d_null && ! val2.d_null) {
      if (d < val2.d)
        k = -1;
      else if (d > val2.d)
        k = +1;
    } else if (! d_null) {
      k = +1;
    } else if (! val2.d_null) {
      k = -1;
    }
    if (k == 0)
      n++;
  }
  require(n <= numattrs);
  if (num_eq != 0)
    *num_eq = n;
  return k;
}

void
Val::fromib(const NdbIndexScanOperation::IndexBound& ib, uint j)
{
  const char* key = (j == 0 ? ib.low_key : ib.high_key);
  const uint numattrs = (j == 0 ? ib.low_key_count : ib.high_key_count);
  const Uint8 nullbits = *(const Uint8*)key;
  require(numattrs <= g_numattrs);
  if (numattrs >= 1) {
    if (nullbits & (1 << g_ndbrec_b_nb_offset))
      b_null = 1;
    else {
      memcpy(&b, &key[g_ndbrec_b_offset], sizeof(b));
      b_null = 0;
    }
  }
  if (numattrs >= 2) {
    if (nullbits & (1 << g_ndbrec_c_nb_offset))
      c_null = 1;
    else {
      memcpy(c, &key[g_ndbrec_c_offset], sizeof(c));
      c_null = 0;
    }
  }
  if (numattrs >= 3) {
    if (nullbits & (1 << g_ndbrec_d_nb_offset))
      d_null = 1;
    else {
      memcpy(&d, &key[g_ndbrec_d_offset], sizeof(d));
      d_null = 0;
    }
  }
  m_numattrs = numattrs;
}

// index keys

struct Key {
  Val m_val;
  int8 m_flag; // temp use
  Key();

private:
  Key& operator=(const Key&);
  Key(const Key&);
};

static NdbOut&
operator<<(NdbOut& out, const Key& key)
{
  out << key.m_val;
  if (key.m_flag != -1)
    out << " flag: " << key.m_flag;
  return out;
}

Key::Key()
{
  m_flag = -1;
}

static Key* g_keys = 0;
static uint* g_sortkeys = 0;

static void
freekeys()
{
  delete [] g_keys;
  delete [] g_sortkeys;
  g_keys = 0;
  g_sortkeys = 0;
}

static void
allockeys()
{
  freekeys();
  g_keys = new Key [g_opts.rows];
  g_sortkeys = new uint [g_opts.rows];
  require(g_keys != 0 && g_sortkeys != 0);
  memset(g_sortkeys, 0xff, sizeof(uint) * g_opts.rows);
}

static int
cmpkeys(const void* p1, const void* p2)
{
  const uint i1 = *(const uint*)p1;
  const uint i2 = *(const uint*)p2;
  require(i1 < g_opts.rows && i2 < g_opts.rows);
  const Key& key1 = g_keys[i1];
  const Key& key2 = g_keys[i2];
  const int k = key1.m_val.cmp(key2.m_val, g_opts.attrs);
  return k;
}

static void
sortkeys()
{
  ll2("sortkeys");
  uint i;

  // sort
  for (i = 0; i < g_opts.rows; i++)
    g_sortkeys[i] = i;
  qsort(g_sortkeys, g_opts.rows, sizeof(uint), cmpkeys);

  // verify
  uint unique = 1;
  for (i = 1; i < g_opts.rows; i++) {
    const uint i1 = g_sortkeys[i - 1];
    const uint i2 = g_sortkeys[i];
    require(i1 < g_opts.rows && i2 < g_opts.rows);
    const Key& key1 = g_keys[i1];
    const Key& key2 = g_keys[i2];
    const int k = key1.m_val.cmp(key2.m_val, g_opts.attrs);
    require(k <= 0);
    if (k < 0)
      unique++;
  }

  // show min max key
  ll1("minkey:" << g_keys[g_sortkeys[0]]);
  ll1("maxkey:" << g_keys[g_sortkeys[g_opts.rows - 1]]);
  ll1("unique:" << unique);
}

static void
makekeys()
{
  ll1("makekeys");

  uint initrows = g_opts.rows / g_opts.rpk;
  require(initrows != 0);

  // distinct keys
  uint i = 0;
  while (i < initrows) {
    Key& key = g_keys[i];
    key.m_val.make(g_numattrs, g_lim_val);
    i++;
  }

  // remaining keys
  while (i < g_opts.rows) {
    // if rpkvar is 10, multiply rpk by number between 0.1 and 10.0
    double a = (double)(1 + urandom(g_opts.rpkvar * g_opts.rpkvar));
    double b = a / (double)g_opts.rpkvar;
    double c = b * (double)g_opts.rpk;
    const uint n = (uint)(c + 0.5);
    // select random key to duplicate from initrows
    const uint k = urandom(initrows);
    uint j = 0;
    while (i < g_opts.rows && j < n) {
      g_keys[i].m_val.copy(g_keys[k].m_val);
      j++;
      i++;
    }
  }

  // shuffle
  i = 0;
  while (i < g_opts.rows) {
    uint j = urandom(g_opts.rows);
    if (i != j) {
      Key tmp;
      tmp.m_val.copy(g_keys[i].m_val);
      g_keys[i].m_val.copy(g_keys[j].m_val);
      g_keys[j].m_val.copy(tmp.m_val);
    }
    i++;
  }

  // sort
  sortkeys();
}

// data loading

static int
verifydata()
{
  ll3("verifydata");
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  chkdb((g_scan_op = g_con->getNdbScanOperation(g_tab)) != 0);
  chkdb(g_scan_op->readTuples(NdbScanOperation::LM_CommittedRead) == 0);
  Uint32 a;
  Val val;
  val.m_numattrs = g_numattrs;
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
  for (i = 0; i < g_opts.rows; i++) {
    Key& key = g_keys[i];
    key.m_flag = false; // not scanned
  }
  while (1) {
    int ret;
    a = ~(Uint32)0;
    chkdb((ret = g_scan_op->nextResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    val.b_null = b_ra->isNULL();
    val.c_null = c_ra->isNULL();
    val.d_null = d_ra->isNULL();
    require(val.b_null == 0 || (g_b_nullable && val.b_null == 1));
    require(val.c_null == 0 || (g_c_nullable && val.c_null == 1));
    require(val.d_null == 0 || (g_d_nullable && val.d_null == 1));
    i = (uint)a;
    chkrc(i < g_opts.rows);
    Key& key = g_keys[i];
    chkrc(key.m_val.cmp(val) == 0);
    chkrc(key.m_flag == false);
    key.m_flag = true;
    count++;
  }
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_scan_op = 0;
  for (i = 0; i < g_opts.rows; i++) {
    Key& key = g_keys[i];
    chkrc(key.m_flag == true);
    key.m_flag = -1; // forget
  }
  require(count == g_opts.rows);
  ll3("verifydata: " << g_opts.rows << " rows");
  return 0;
}

static int
loaddata(bool update)
{
  ll1("loaddata: update: " << update);
  const uint batch = 512;
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  uint i = 0;
  while (i < g_opts.rows) {
    chkdb((g_op = g_con->getNdbOperation(g_tab)) != 0);
    if (!update)
      chkdb(g_op->insertTuple() == 0);
    else
      chkdb(g_op->updateTuple() == 0);
    Uint32 a = i;
    const Val& val = g_keys[i].m_val;
    const char* a_addr = (const char*)&a;
    const char* b_addr = ! val.b_null ? (const char*)&val.b : 0;
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

  // check data and cmp routines
  chkrc(verifydata() == 0);

  for (uint i = 0; i < g_opts.rows; i++)
    ll3("load " << i << ": " << g_keys[i]);
  ll0("loaddata: " << g_opts.rows << " rows");
  return 0;
}

// bounds

struct Bnd {
  Val m_val;
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
  int8 m_side;
  int8 m_lohi; // 0-lo 1-hi as part of Rng
  Bnd();
  bool isempty() const;
  void copy(const Bnd& bnd2); // does not copy m_lohi
  Bnd& make(uint minattrs);
  Bnd& make(uint minattrs, const Val& theval);
  int cmp(const Key& key) const;
  int cmp(const Bnd& bnd2);
  int type(uint colno) const; // for setBound
  void fromib(const NdbIndexScanOperation::IndexBound& ib, uint j);

private:
  Bnd& operator=(const Bnd&);
  Bnd(const Bnd&);
};

static NdbOut&
operator<<(NdbOut& out, const Bnd& bnd)
{
  if (bnd.m_lohi == 0)
    out << "L";
  else if (bnd.m_lohi == 1)
    out << "H";
  else
    out << bnd.m_lohi << "?";
  out << bnd.m_val;
  if (bnd.m_side == 0)
    ;
  else if (bnd.m_side == -1)
    out << "-";
  else if (bnd.m_side == +1)
    out << "+";
  return out;
}

Bnd::Bnd()
{
  m_side = 0;
  m_lohi = -1;
}

bool
Bnd::isempty() const
{
  return m_val.m_numattrs == 0;
}

void
Bnd::copy(const Bnd& bnd2)
{
  m_val.copy(bnd2.m_val);
  m_side = bnd2.m_side;
}

Bnd&
Bnd::make(uint minattrs)
{
  require(minattrs <= g_opts.attrs);
  require(m_lohi == 0 || m_lohi == 1);
  uint numattrs = minattrs + urandom(g_numattrs - minattrs + 1);
  m_val.make(numattrs, g_lim_bnd);
  m_side = m_val.m_numattrs == 0 ? 0 : urandom(2) == 0 ? -1 : +1;
  return *this;
}

Bnd&
Bnd::make(uint minattrs, const Val& theval)
{
  uint numattrs = minattrs + urandom(g_numattrs - minattrs);
  m_val.copy(theval);
  m_val.m_numattrs = numattrs;
  m_side = m_val.m_numattrs == 0 ? 0 : urandom(2) == 0 ? -1 : +1;
  return *this;
}

int
Bnd::cmp(const Key& key) const
{
  int place; // debug
  int ret;
  do {
    int k = key.m_val.cmp(m_val, m_val.m_numattrs);
    if (k != 0) {
      place = 1;
      ret = k;
      break;
    }
    if (m_side != 0) {
      place = 2;
      ret = (-1) * m_side;
      break;
    }
    place = 3;
    ret = 0;
    require(m_val.m_numattrs == 0);
  } while (0);
  ll3("bnd: " << *this << " cmp key: " << key
      << " ret: " << ret << " place: " << place);
  return ret;
}

int
Bnd::cmp(const Bnd& bnd2)
{
  int place; // debug
  int ret;
  const Bnd& bnd1 = *this;
  const Val& val1 = bnd1.m_val;
  const Val& val2 = bnd2.m_val;
  const uint numattrs1 = val1.m_numattrs;
  const uint numattrs2 = val2.m_numattrs;
  const uint n = (numattrs1 < numattrs2 ? numattrs1 : numattrs2);
  do {
    int k = val1.cmp(val2, n);
    if (k != 0) {
      place = 1;
      ret = k;
      break;
    }
    if (numattrs1 < numattrs2) {
      place = 2;
      ret = (+1) * bnd1.m_side;
      break;
    }
    if (numattrs1 > numattrs2) {
      place = 3;
      ret = (-1) * bnd1.m_side;
      break;
    }
    if (bnd1.m_side < bnd2.m_side) {
      place = 4;
      ret = -1;
      break;
    }
    if (bnd1.m_side > bnd2.m_side) {
      place = 5;
      ret = +1;
      break;
    }
    place = 6;
    ret = 0;
  } while (0);
  ll3("bnd: " << *this << " cmp bnd: " << bnd2
      << " ret: " << ret << " place: " << place);
  return ret;
}

int
Bnd::type(uint colno) const
{
  int t;
  require(colno < m_val.m_numattrs && (m_side == -1 || m_side == +1));
  require(m_lohi == 0 || m_lohi == 1);
  if (m_lohi == 0) {
    if (colno + 1 < m_val.m_numattrs)
      t = 0; // LE
    else if (m_side == -1)
      t = 0; // LE
    else
      t = 1; // LT
  } else {
    if (colno + 1 < m_val.m_numattrs)
      t = 2; // GE
    else if (m_side == +1)
      t = 2; // GE
    else
      t = 3; // GT
  }
  return t;
}

void
Bnd::fromib(const NdbIndexScanOperation::IndexBound& ib, uint j)
{
  Val& val = m_val;
  val.fromib(ib, j);
  const uint numattrs = (j == 0 ? ib.low_key_count : ib.high_key_count);
  const bool inclusive = (j == 0 ? ib.low_inclusive : ib.high_inclusive);
  if (numattrs == 0) {
    m_side = 0;
  } else {
    m_side = (j == 0 ? (inclusive ? -1 : +1) : (inclusive ? +1 : -1));
  }
  m_lohi = j;
}

// stats values

struct Stval {
  Uint32 rir_v2;
  double rir;
  double rpk[g_numattrs];
  bool empty;
  char rule[NdbIndexStat::RuleBufferBytes];
  Stval();
};

static NdbOut&
operator<<(NdbOut& out, const Stval& st)
{
  out << "rir_v2: " << st.rir_v2;
  out << " rir_v4: " << st.rir;
  out << " rpk:[ ";
  for (uint k = 0; k < g_opts.attrs; k++) {
    if (k != 0)
      out << " ";
    out << st.rpk[k];
  }
  out << " ]";
  out << " " << (st.empty ? "E" : "N");
  out << " " << st.rule;
  return out;
}

Stval::Stval()
{
  rir_v2 = 0;
  rir = 0.0;
  for (uint k = 0; k < g_numattrs; k++)
    rpk[k] = 0.0;
  empty = false;
  strcpy(rule, "-");
}

// ranges

struct Rng {
  Bnd m_bnd[2];
  Int32 m_rowcount;
  // stats v2
  double errpct;
  // stats v4
  Stval m_st_scan; // exact stats computed from keys in range
  Stval m_st_stat; // interpolated kernel stats via g_is
  Rng();
  uint minattrs() const;
  uint maxattrs() const;
  bool iseq() const;
  bool isempty() const;
  void copy(const Rng& rng2);
  int cmp(const Key& key) const; // -1,0,+1 = key is before,in,after range
  uint rowcount() const;
  void fromib(const NdbIndexScanOperation::IndexBound& ib);

private:
  Rng& operator=(const Rng&);
  Rng(const Rng&);
};

static NdbOut&
operator<<(NdbOut& out, const Rng& rng)
{
  out << rng.m_bnd[0] << " " << rng.m_bnd[1];
  if (rng.m_rowcount != -1)
    out << " rows: " << rng.m_rowcount;
  return out;
}

Rng::Rng()
{
  m_bnd[0].m_lohi = 0;
  m_bnd[1].m_lohi = 1;
  m_rowcount = -1;
}

uint
Rng::minattrs() const
{
  return min(m_bnd[0].m_val.m_numattrs, m_bnd[1].m_val.m_numattrs);
}

uint
Rng::maxattrs() const
{
  return max(m_bnd[0].m_val.m_numattrs, m_bnd[1].m_val.m_numattrs);
}

bool
Rng::iseq() const
{
  return
    minattrs() == maxattrs() &&
    m_bnd[0].m_val.cmp(m_bnd[1].m_val, minattrs()) == 0 &&
    m_bnd[0].m_side < m_bnd[1].m_side;
}

bool
Rng::isempty() const
{
  return m_bnd[0].isempty() && m_bnd[1].isempty();
}

void
Rng::copy(const Rng& rng2)
{
  m_bnd[0].copy(rng2.m_bnd[0]);
  m_bnd[1].copy(rng2.m_bnd[1]);
  m_rowcount = rng2.m_rowcount;
}

int
Rng::cmp(const Key& key) const
{
  int place; // debug
  int ret;
  do  {
    int k;
    k = m_bnd[0].cmp(key);
    if (k < 0) {
      place = 1;
      ret = -1;
      break;
    }
    k = m_bnd[1].cmp(key);
    if (k > 0) {
      place = 2;
      ret = +1;
      break;
    }
    place = 3;
    ret = 0;
  } while (0);
  ll3("rng: " << *this << " cmp key: " << key
      << " ret: " << ret << " place: " << place);
  return ret;
}

uint
Rng::rowcount() const
{
  ll3("rowcount: " << *this);
  int i;
  // binary search for first and last in range
  int lim[2];
  for (i = 0; i <= 1; i++) {
    ll3("search i=" << i);
    int lo = -1;
    int hi = (int)g_opts.rows;
    int ret;
    int j;
    do {
      j = (hi + lo) / 2;
      require(lo < j && j < hi);
      ret = cmp(g_keys[g_sortkeys[j]]);
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

  // verify is expensive due to makeranges() multiple tries
  const bool verify = (urandom(10) == 0);
  const int lo = max(lim[0], 0);
  const int hi = min(lim[1], (int)g_opts.rows - 1);
  if (verify) {
    int pos = -1; // before, within, after
    for (i = 0; i < (int)g_opts.rows; i++) {
      int k = cmp(g_keys[g_sortkeys[i]]);
      if (k < 0)
        require(i < lo);
      else if (k == 0)
        require(lo <= i && i <= hi);
      else
        require(i > hi);
      require(pos <= k);
      if (pos < k)
        pos = k;
    }
  }

  // result
  require(hi - lo + 1 >= 0);
  uint count = hi - lo + 1;
  ll3("rowcount: " << count << " lim: " << lim[0] << " " << lim[1]);
  return count;
}

void
Rng::fromib(const NdbIndexScanOperation::IndexBound& ib)
{
  for (uint j = 0; j <= 1; j++) {
    Bnd& bnd = m_bnd[j];
    bnd.fromib(ib, j);
  }
}

static Rng* g_rnglist = 0;

static void
freeranges()
{
  delete [] g_rnglist;
  g_rnglist = 0;
}

static void
allocranges()
{
  freeranges();
  g_rnglist = new Rng [g_opts.ops];
  require(g_rnglist != 0);
}

static void
makeranges()
{
  ll1("makeranges");
  const uint mintries = 20;
  const uint maxtries = 80;
  const uint fudgefac = 10;

  for (uint i = 0; i < g_opts.ops; i++) {
    const bool eqpart = (urandom(100) < g_opts.eqscans);
    const bool eqfull = eqpart && (urandom(100) < g_opts.eqscans);
    Rng rng; // candidate
    uint j;
    for (j = 0; j < maxtries; j++) {
      Rng rng2;
      if (!eqpart) {
        rng2.m_bnd[0].make(0);
        rng2.m_bnd[1].make(0);
      } else {
        const uint mincnt = eqfull ? g_opts.attrs : 1;
        rng2.m_bnd[0].make(mincnt);
        rng2.m_bnd[1].copy(rng2.m_bnd[0]);
        rng2.m_bnd[0].m_side = -1;
        rng2.m_bnd[1].m_side = +1;
        require(rng2.iseq());
      }
      rng2.m_rowcount = (Int32)rng2.rowcount();
      // 0-discard 1-replace or accept 2-accept
      int action = 0;
      do {
        // first candidate
        if (rng.m_rowcount == -1) {
          action = 1;
          break;
        }
        require(rng.m_rowcount != -1);
        // prefer some bounds
        if (rng2.isempty()) {
          if (urandom(fudgefac) != 0)
            action = 0;
          else
            action = 1;
          break;
        }
        // prefer some rows
        if (rng2.m_rowcount == 0) {
          action = 0;
          break;
        }
        // accept if row count under given pct
        require((uint)rng2.m_rowcount <= g_opts.rows);
        if (100 * (uint)rng2.m_rowcount <= g_opts.scanpct * g_opts.rows) {
          if (urandom(fudgefac) != 0) {
            action = 2;
            break;
          }
        }
        // replace if less rows
        if (rng2.m_rowcount < rng.m_rowcount) {
          if (urandom(fudgefac) != 0) {
            action = 1;
            break;
          }
        }
      } while (0);
      if (action != 0) {
        rng.copy(rng2);
        if (action == 2 || j >= mintries)
          break;
      }
    }
    g_rnglist[i].copy(rng);
    ll2("rng " << i << ": " << rng << " tries: " << j);
  }
}

// verify ranges via range scans

static int
setbounds(const Rng& rng)
{
  // currently must do each attr in order
  ll3("setbounds: " << rng);
  uint i;
  const Bnd (&bnd)[2] = rng.m_bnd;
  for (i = 0; i < g_numattrs; i++) {
    const Uint32 no = i; // index attribute number
    uint j;
    int type[2] = { -1, -1 };
    // determine inclusivity (boundtype) of upper+lower bounds on this col.
    // -1 == no bound on the col.
    for (j = 0; j <= 1; j++) {
      if (no < bnd[j].m_val.m_numattrs)
        type[j] = bnd[j].type(no);
    }
    for (j = 0; j <= 1; j++) {
      int t = type[j];
      if (t == -1)
        continue;
      if (no + 1 < bnd[j].m_val.m_numattrs)
        t &= ~(uint)1; // strict bit is set on last bound only
      const Val& val = bnd[j].m_val;
      const void* addr = 0;
      if (no == 0)
        addr = ! val.b_null ? (const void*)&val.b : 0;
      else if (no == 1)
        addr = ! val.c_null ? (const void*)val.c : 0;
      else if (no == 2)
        addr = ! val.d_null ? (const void*)&val.d : 0;
      else
        require(false);
      ll3("setBound attr:" << no << " type:" << t << " val: " << val);
      chkdb(g_rangescan_op->setBound(no, t, addr) == 0);
    }
  }
  return 0;
}

static int
scanrange(const Rng& rng)
{
  ll3("scanrange: " << rng);
  chkdb((g_con = g_ndb->startTransaction()) != 0);
  chkdb((g_rangescan_op = g_con->getNdbIndexScanOperation(g_ind, g_tab)) != 0);
  chkdb(g_rangescan_op->readTuples() == 0);
  chkrc(setbounds(rng) == 0);
  Uint32 a;
  char* a_addr = (char*)&a;
  Uint32 no = 0;
  chkdb(g_rangescan_op->getValue(no++, a_addr) != 0);
  chkdb(g_con->execute(NdbTransaction::NoCommit) == 0);
  uint count = 0;
  uint i;
  for (i = 0; i < g_opts.rows; i++) {
    Key& key = g_keys[i];
    key.m_flag = false; // not scanned
  }
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
    int k = rng.cmp(key);
    chkrc(k == 0);
    chkrc(key.m_flag == false);
    key.m_flag = true;
    count++;
  }
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_rangescan_op = 0;

  for (i = 0; i < g_opts.rows; i++) {
    Key& key = g_keys[i];
    int k = rng.cmp(key);
    if (k != 0) // not in range
      chkrc(key.m_flag == false);
    else
      chkrc(key.m_flag == true);
    key.m_flag = -1; // forget
  }
  require((uint)rng.m_rowcount == count);
  return 0;
}

static int
scanranges()
{
  ll1("scanranges");
  for (uint i = 0; i < g_opts.ops; i++) {
    const Rng& rng = g_rnglist[i];
    chkrc(scanrange(rng) == 0);
  }
  return 0;
}

// stats v4 update

static int
definestat()
{
  ll1("definestat");
  require(g_is != 0 && g_ind != 0 && g_tab != 0);
  chkdb(g_is->set_index(*g_ind, *g_tab) == 0);
  return 0;
}

static int
updatestat()
{
  ll1("updatestat");
  if (urandom(2) == 0) {
    g_dic = g_ndb->getDictionary();
    chkdb(g_dic->updateIndexStat(*g_ind, *g_tab) == 0);
    g_dic = 0;
  } else {
    chkdb(g_is->update_stat(g_ndb_sys) == 0);
  }
  return 0;
}

static int
readstat()
{
  ll1("readstat");

  NdbIndexStat::Head head;
  chkdb(g_is->read_head(g_ndb_sys) == 0);
  g_is->get_head(head);
  chkrc(head.m_found == true);
  chkrc(head.m_sampleVersion != 0);
  ll1("readstat:"
      << " sampleVersion: " << head.m_sampleVersion
      << " sampleCount: " << head.m_sampleCount);

  NdbIndexStat::CacheInfo infoQuery;
  chkdb(g_is->read_stat(g_ndb_sys) == 0);
  g_is->move_cache();
  g_is->get_cache_info(infoQuery, NdbIndexStat::CacheQuery);
  ll1("readstat: cache bytes: " << infoQuery.m_totalBytes);
  return 0;
}

// test polling after updatestat

static int
startlistener()
{
  ll1("startlistener");
  chkdb(g_is->create_listener(g_ndb_sys) == 0);
  chkdb(g_is->execute_listener(g_ndb_sys) == 0);
  return 0;
}

static int
runlistener()
{
  ll1("runlistener");
  int ret;
  chkdb((ret = g_is->poll_listener(g_ndb_sys, 10000)) != -1);
  chkrc(ret == 1);
  // one event is expected
  chkdb((ret = g_is->next_listener(g_ndb_sys)) != -1);
  chkrc(ret == 1);
  chkdb((ret = g_is->next_listener(g_ndb_sys)) != -1);
  chkrc(ret == 0);
  return 0;
}

static int
stoplistener()
{
  ll1("stoplistener");
  chkdb(g_is->drop_listener(g_ndb_sys) != -1);
  return 0;
}

// stats queries

// exact stats from scan results
static void
queryscan(Rng& rng)
{
  ll3("queryscan");

  uint rir;
  uint unq[g_numattrs];
  rir = 0;
  for (uint k = 0; k < g_opts.attrs; k++)
    unq[0] = 0;
  Key prevkey;
  for (uint i = 0; i < g_opts.rows; i++) {
    const Key& key = g_keys[g_sortkeys[i]];
    int res = rng.cmp(key);
    if (res != 0)
      continue;
    rir++;
    if (rir == 1) {
      for (uint k = 0; k < g_opts.attrs; k++)
        unq[k] = 1;
    } else {
      uint num_eq = ~0;
      int res = prevkey.m_val.cmp(key.m_val, g_opts.attrs, &num_eq);
      if (res == 0)
        require(num_eq == g_opts.attrs);
      else {
        require(res < 0);
        require(num_eq < g_opts.attrs);
        unq[num_eq]++;
        // propagate down
        for (uint k = num_eq + 1; k < g_opts.attrs; k++)
          unq[k]++;
      }
    }
    prevkey.m_val.copy(key.m_val);
  }
  require(rng.m_rowcount != -1);
  require((uint)rng.m_rowcount == rir);

  Stval& st = rng.m_st_scan;
  st.rir_v2 = rir;
  st.rir = rir == 0 ? 1.0 : (double)rir;
  for (uint k = 0; k < g_opts.attrs; k++) {
    if (rir == 0)
      st.rpk[k] = 1.0;
    else {
      require(rir >= unq[k]);
      require(unq[k] != 0);
      st.rpk[k] = (double)rir / (double)unq[k];
    }
  }
  st.empty = (rir == 0);
  ll2("queryscan: " << st);
}

/* This method initialises the passed in IndexBound
 * to represent the range passed in.
 * It assumes that the storage pointed to by low_key
 * and high_key in the passed IndexBound can be overwritten
 * and is long enough to store the data
 */
static int
initialiseIndexBound(const Rng& rng, 
                     NdbIndexScanOperation::IndexBound& ib,
                     my_record* low_key, my_record* high_key)
{
  ll3("initialiseIndexBound: " << rng);
  uint i;
  const Bnd (&bnd)[2] = rng.m_bnd;
  Uint32 colsInBound[2]= {0, 0};
  bool boundInclusive[2]= {false, false};

  memset(&ib, 0xf1, sizeof(ib));
  memset(low_key, 0xf2, sizeof(*low_key));
  memset(high_key, 0xf3, sizeof(*high_key));

  // Clear nullbit storage
  low_key->m_null_bm = 0;
  high_key->m_null_bm = 0;

  for (i = 0; i < g_numattrs; i++) {
    const Uint32 no = i; // index attribute number
    uint j;
    int type[2] = { -1, -1 };
    // determine inclusivity (boundtype) of upper+lower bounds on this col.
    // -1 == no bound on the col.
    for (j = 0; j <= 1; j++) {
      if (no < bnd[j].m_val.m_numattrs)
        type[j] = bnd[j].type(no);
    }
    for (j = 0; j <= 1; j++) {
      /* Get ptr to key storage space for this bound */
      my_record* keyBuf= (j==0) ? low_key : high_key;
      int t = type[j];
      if (t == -1)
        continue;
      colsInBound[j]++;

      if (no + 1 >= bnd[j].m_val.m_numattrs)
        // Last column in bound, inclusive if GE or LE (or EQ)
        // i.e. bottom bit of boundtype is clear
        boundInclusive[j]= !(t & 1);
      
      const Val& val = bnd[j].m_val;
      if (no == 0)
      {
        if (! val.b_null)
          keyBuf->m_b= val.b;

        if (g_b_nullable)
          keyBuf->m_null_bm |= ((val.b_null?1:0) << g_ndbrec_b_nb_offset);
      }
      else if (no == 1)
      {
        if (! val.c_null)
          memcpy(&keyBuf->m_c[0], (const void*)&val.c, 1+ g_charlen);
        
        if (g_c_nullable)
          keyBuf->m_null_bm |= ((val.c_null?1:0) << g_ndbrec_c_nb_offset);
      } 
      else if (no == 2)
      {
        if (! val.d_null)
          keyBuf->m_d= val.d;

        if (g_d_nullable)
          keyBuf->m_null_bm |= ((val.d_null?1:0) << g_ndbrec_d_nb_offset);
      }
      else
        require(false);
      ll3("initialiseIndexBound attr:" << no << " type:" << t << " val: " << val);
    }
  }

  /* Now have everything we need to initialise the IndexBound */
  ib.low_key = (char*)low_key;
  ib.low_key_count= colsInBound[0];
  ib.low_inclusive= boundInclusive[0];
  ib.high_key = (char*)high_key;
  ib.high_key_count= colsInBound[1];
  ib.high_inclusive= boundInclusive[1];
  ib.range_no= 0;

  ll3(" indexBound low_key_count=" << ib.low_key_count << 
      " low_inc=" << ib.low_inclusive <<
      " high_key_count=" << ib.high_key_count <<
      " high_inc=" << ib.high_inclusive);
  ll3(" low bound b=" << *((Uint32*) &ib.low_key[g_ndbrec_b_offset]) <<
      " d=" << *((Uint16*) &ib.low_key[g_ndbrec_d_offset]) <<
      " first byte=" << ib.low_key[0]);
  ll3(" high bound b=" << *((Uint32*) &ib.high_key[g_ndbrec_b_offset]) <<
      " d=" << *((Uint16*) &ib.high_key[g_ndbrec_d_offset]) <<
      " first byte=" << ib.high_key[0]);  

  // verify by reverse
  {
    Rng rng;
    rng.fromib(ib);
    require(rng.m_bnd[0].cmp(bnd[0]) == 0);
    require(rng.m_bnd[1].cmp(bnd[1]) == 0);
  }
  return 0;
}

static int
querystat_v2(Rng& rng)
{
  ll3("querystat_v2");
  
  /* Create IndexBound and key storage space */
  NdbIndexScanOperation::IndexBound ib;
  my_record low_key;
  my_record high_key;

  chkdb((g_con = g_ndb->startTransaction()) != 0);
  chkrc(initialiseIndexBound(rng, ib, &low_key, &high_key) == 0);

  Uint64 count = ~(Uint64)0;
  chkdb(g_is->records_in_range(g_ind, 
                                 g_con,
                                 g_ind_rec,
                                 g_tab_rec,
                                 &ib,
                                 0,
                                 &count, 
                                 0) == 0);
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_rangescan_op = 0;

  Stval& st = rng.m_st_stat;
  chkrc(count < (1 << 30));
  st.rir_v2 = (Uint32)count;
  ll2("querystat_v2: " << st.rir_v2 << " rows");
  return 0;
}

static int
querystat(Rng& rng)
{
  ll3("querystat");

  // set up range
  Uint8 bound_lo_buffer[NdbIndexStat::BoundBufferBytes];
  Uint8 bound_hi_buffer[NdbIndexStat::BoundBufferBytes];
  NdbIndexStat::Bound bound_lo(g_is, bound_lo_buffer);
  NdbIndexStat::Bound bound_hi(g_is, bound_hi_buffer);
  NdbIndexStat::Range range(bound_lo, bound_hi);

  // convert to IndexBound (like in mysqld)
  NdbIndexScanOperation::IndexBound ib;
  my_record low_key;
  my_record high_key;
  chkrc(initialiseIndexBound(rng, ib, &low_key, &high_key) == 0);
  chkrc(g_is->convert_range(range, g_ind_rec, &ib) == 0);

  // index stat query
  Uint8 stat_buffer[NdbIndexStat::StatBufferBytes];
  NdbIndexStat::Stat stat(stat_buffer);
  chkdb(g_is->query_stat(range, stat) == 0);

  // save result
  Stval& st = rng.m_st_stat;
  g_is->get_rir(stat, &st.rir);
  for (uint k = 0; k < g_opts.attrs; k++) {
    g_is->get_rpk(stat, k, &st.rpk[k]);
  }
  g_is->get_empty(stat, &st.empty);
  g_is->get_rule(stat, st.rule);

  ll2("querystat: " << st);
  return 0;
}

static int
queryranges()
{
  ll2("queryranges");
  for (uint i = 0; i < g_opts.ops; i++) {
    Rng& rng = g_rnglist[i];
    ll1("rng " << i << ": " << rng);
    // exact stats
    queryscan(rng);
    // interpolated stats
    chkrc(querystat_v2(rng) == 0);
    chkrc(querystat(rng) == 0);
    const Stval& st1 = rng.m_st_scan;
    const Stval& st2 = rng.m_st_stat;
    // if rir v2 is zero then it is exact
    chkrc(st2.rir_v2 != 0 || st1.rir_v2 == 0);
  }
  return 0;
}

// general statistics methods

struct Stats : public NDBT_Stats {
  Stats();
  void add(double x2);
  void add(const Stats& sum2);
};

static NdbOut&
operator<<(NdbOut& out, const Stats& st)
{
  out << "count: " << st.getCount()
      << " min: " << st.getMin()
      << " max: " << st.getMax()
      << " mean: " << st.getMean()
      << " stddev: " << st.getStddev();
  return out;
}

Stats::Stats()
{
}

void
Stats::add(double x2)
{
  addObservation(x2);
}

void
Stats::add(const Stats& st2)
{
  *this += st2;
}

// error statistics scan vs stat

struct Sterr {
  Stats rir_v2;
  Stats rir;
  Stats rpk[g_numattrs];
  Sterr();
  void add(const Sterr& st2);
};

static NdbOut&
operator<<(NdbOut& out, const Sterr& st)
{
  out << "rir_v2: " << st.rir_v2 << endl;
  out << "rir_v4: " << st.rir;
  for (uint k = 0; k < g_opts.attrs; k++) {
    out << endl << "rpk[" << k << "]: " << st.rpk[k];
  }
  return out;
}

Sterr::Sterr()
{
}

void
Sterr::add(const Sterr& st2)
{
  rir_v2.add(st2.rir_v2);
  rir.add(st2.rir);
  for (uint k = 0; k < g_opts.attrs; k++) {
    rpk[k].add(st2.rpk[k]);
  }
}

static void
sumrange(const Rng& rng, Sterr& st)
{
  const Stval& st1 = rng.m_st_scan;
  const Stval& st2 = rng.m_st_stat;

  // rir_v2 error as pct of total rows
  {
    double rows = (double)g_opts.rows;
    double x1 = (double)st1.rir_v2;
    double x2 = (double)st2.rir_v2;
    double x3 = 100.0 * (x2 - x1) / rows;
    st.rir_v2.add(x3);
  }

  // rir error as pct of total rows
  {
    double rows = (double)g_opts.rows;
    double x1 = st1.rir;
    double x2 = st2.rir;
    double x3 = 100.0 * (x2 - x1) / rows;
    st.rir.add(x3);
  }

  // rpk errors as plain diff
  for (uint k = 0; k < g_opts.attrs; k++) {
    double x1 = st1.rpk[k];
    double x2 = st2.rpk[k];
    double x3 = (x2 - x1);
    st.rpk[k].add(x3);
  }
}

static void
sumranges(Sterr& st)
{
  for (uint i = 0; i < g_opts.ops; i++) {
    const Rng& rng = g_rnglist[i];
    sumrange(rng, st);
  }
}

// loop and final stats

static Sterr g_sterr;

static void
loopstats()
{
  Sterr st;
  sumranges(st);
  if (g_opts.loops != 1) {
    ll0("=== loop " << g_loop << " summary ===");
    ll0(st);
  }
  // accumulate
  g_sterr.add(st);
}

static int
loopdumps()
{
  char file[200];
  if (g_opts.dump == 0)
    return 0;
  {
    BaseString::snprintf(file, sizeof(file),
                         "%s.key.%d", g_opts.dump, g_loop);
    FILE* f = 0;
    chker((f = fopen(file, "w")) != 0);
    fprintf(f, "a");
    for (uint k = 0; k < g_opts.attrs; k++) {
      if (k == 0)
        fprintf(f, ",b_null,b");
      else if (k == 1)
        fprintf(f, ",c_null,c");
      else if (k == 2)
        fprintf(f, ",d_null,d");
      else
        require(false);
    }
    fprintf(f, "\n");
    for (uint i = 0; i < g_opts.rows; i++) {
      const Key& key = g_keys[g_sortkeys[i]];
      const Val& val = key.m_val;
      fprintf(f, "%u", i);
      for (uint k = 0; k < g_opts.attrs; k++) {
        if (k == 0) {
          fprintf(f, ",%d,", val.b_null);
          if (!val.b_null)
            fprintf(f, "%u", val.b);
        } else if (k == 1) {
          fprintf(f, ",%d,", val.c_null);
          if (!val.c_null)
            fprintf(f, "%.*s", val.c[0], &val.c[1]);
        } else if (k == 2) {
          fprintf(f, ",%d,", val.d_null);
          if (!val.d_null)
            fprintf(f, "%u", val.d);
        } else {
          require(false);
        }
      }
      fprintf(f, "\n");
    }
    chker(fclose(f) == 0);
  }
  {
    BaseString::snprintf(file, sizeof(file),
                         "%s.range.%d", g_opts.dump, g_loop);
    FILE* f = 0;
    chker((f = fopen(file, "w")) != 0);
    fprintf(f, "op");
    for (uint j = 0; j <= 1; j++) {
      const char* suf = (j == 0 ? "_lo" : "_hi");
      fprintf(f, ",attrs%s", suf);
      for (uint k = 0; k < g_opts.attrs; k++) {
        if (k == 0)
          fprintf(f, ",b_null%s,b%s", suf, suf);
        else if (k == 1)
          fprintf(f, ",c_null%s,c%s", suf, suf);
        else if (k == 2)
          fprintf(f, ",d_null%s,d%s", suf, suf);
        else
          require(false);
      }
      fprintf(f, ",side%s", suf);
    }
    fprintf(f, "\n");
    for (uint i = 0; i < g_opts.ops; i++) {
      const Rng& rng = g_rnglist[i];
      fprintf(f, "%u", i);
      for (uint j = 0; j <= 1; j++) {
        const Bnd& bnd = rng.m_bnd[j];
        const Val& val = bnd.m_val;
        fprintf(f, ",%u", val.m_numattrs);
        for (uint k = 0; k < g_opts.attrs; k++) {
          if (k >= val.m_numattrs)
            fprintf(f, ",,");
          else if (k == 0) {
            fprintf(f, ",%d,", val.b_null);
            if (!val.b_null)
              fprintf(f, "%u", val.b);
          } else if (k == 1) {
            fprintf(f, ",%d,", val.c_null);
            if (!val.c_null)
              fprintf(f, "%.*s", val.c[0], &val.c[1]);
          } else if (k == 2) {
            fprintf(f, ",%d,", val.d_null);
            if (!val.d_null)
              fprintf(f, "%u", val.d);
          } else {
            require(false);
          }
        }
        fprintf(f, ",%d", bnd.m_side);
      }
      fprintf(f, "\n");
    }
    chker(fclose(f) == 0);
  }
  {
    BaseString::snprintf(file, sizeof(file),
                         "%s.stat.%d", g_opts.dump, g_loop);
    FILE* f = 0;
    chker((f = fopen(file, "w")) != 0);
    fprintf(f, "op");
    for (uint j = 0; j <= 1; j++) {
      const char* suf = (j == 0 ? "_scan" : "_stat");
      fprintf(f, ",rir_v2%s", suf);
      fprintf(f, ",rir%s", suf);
      for (uint k = 0; k < g_opts.attrs; k++) {
        fprintf(f, ",rpk_%u%s", k, suf);
      }
      fprintf(f, ",empty%s", suf);
      if (j == 1)
        fprintf(f, ",rule%s", suf);
    }
    fprintf(f, "\n");
    for (uint i = 0; i < g_opts.ops; i++) {
      const Rng& rng = g_rnglist[i];
      fprintf(f, "%u", i);
      for (uint j = 0; j <= 1; j++) {
        const Stval& st = (j == 0 ? rng.m_st_scan : rng.m_st_stat);
        fprintf(f, ",%u", st.rir_v2);
        fprintf(f, ",%.2f", st.rir);
        for (uint k = 0; k < g_opts.attrs; k++) {
          fprintf(f, ",%.2f", st.rpk[k]);
        }
        fprintf(f, ",%d", st.empty);
        if (j == 1)
          fprintf(f, ",%s", st.rule);
      }
      fprintf(f, "\n");
    }
    chker(fclose(f) == 0);
  }
  return 0;
}

static void
finalstats()
{
  ll0("=== summary ===");
  ll0(g_sterr);
}

static int
runtest()
{
  ll1("sizeof Val: " << sizeof(Val));
  ll1("sizeof Key: " << sizeof(Key));
  ll1("sizeof Bnd: " << sizeof(Bnd));
  ll1("sizeof Rng: " << sizeof(Rng));

  uint seed = g_opts.seed;
  if (seed != 1) { // not loop number
    if (seed == 0) { // random
      seed = 2 + NdbHost_GetProcessId();
    }
    ll0("random seed is " << seed);
    srand(seed);
  } else {
    ll0("random seed is " << "loop number");
  }
  g_cs = get_charset_by_name(g_csname, MYF(0));
  if (g_cs == 0)
    g_cs = get_charset_by_csname(g_csname, MY_CS_PRIMARY, MYF(0));
  chkrc(g_cs != 0);

  allockeys();
  allocranges();
  chkrc(createtable() == 0);
  chkrc(createindex() == 0);
  chkrc(createNdbRecords() == 0);
  chkrc(definestat() == 0);
  chkrc(startlistener() == 0);

  for (g_loop = 0; g_opts.loops == 0 || g_loop < g_opts.loops; g_loop++) {
    ll0("=== loop " << g_loop << " ===");
    uint seed = g_opts.seed;
    if (seed == 1) { // loop number
      seed = g_loop;
      srand(seed);
    }
    makekeys();
    chkrc(loaddata(g_loop != 0) == 0);
    makeranges();
    chkrc(scanranges() == 0);
    chkrc(updatestat() == 0);
    chkrc(runlistener() == 0);
    chkrc(readstat() == 0);
    chkrc(queryranges() == 0);
    loopstats();
    chkrc(loopdumps() == 0);
  }
  finalstats();

  chkrc(stoplistener() == 0);
  if (!g_opts.keeptable)
    chkrc(droptable() == 0);
  freeranges();
  freekeys();
  return 0;
}

static int
doconnect()
{
  g_ncc = new Ndb_cluster_connection();
  require(g_ncc != 0);
  chkdb(g_ncc->connect(30) == 0);
  g_ndb = new Ndb(g_ncc, "TEST_DB");
  require(g_ndb != 0);
  chkdb(g_ndb->init() == 0 && g_ndb->waitUntilReady(30) == 0);
  g_ndb_sys = new Ndb(g_ncc, "mysql");
  require(g_ndb_sys != 0);
  chkdb(g_ndb_sys->init() == 0 && g_ndb_sys->waitUntilReady(30) == 0);
  g_is = new NdbIndexStat;
  require(g_is != 0);
  return 0;
}

static void
dodisconnect()
{
  delete g_is;
  delete g_ndb_sys;
  delete g_ndb;
  delete g_ncc;
}

static struct my_option
my_long_options[] =
{
  NDB_STD_OPTS("testIndexStat"),
  { "loglevel", NDB_OPT_NOSHORT,
    "Logging level in this program 0-3 (default 0)",
    (uchar **)&g_opts.loglevel, (uchar **)&g_opts.loglevel, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "seed", NDB_OPT_NOSHORT, "Random seed (default 0=random, 1=loop number)",
    (uchar **)&g_opts.seed, (uchar **)&g_opts.seed, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "loops", NDB_OPT_NOSHORT, "Number of test loops (default 1, 0=forever)",
    (uchar **)&g_opts.loops, (uchar **)&g_opts.loops, 0,
    GET_INT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0 },
  { "rows", NDB_OPT_NOSHORT, "Number of rows (default 10000)",
    (uchar **)&g_opts.rows, (uchar **)&g_opts.rows, 0,
    GET_UINT, REQUIRED_ARG, 100000, 0, 0, 0, 0, 0 },
  { "ops", NDB_OPT_NOSHORT,"Number of index scans per loop (default 100)",
    (uchar **)&g_opts.ops, (uchar **)&g_opts.ops, 0,
    GET_UINT, REQUIRED_ARG, 1000, 0, 0, 0, 0, 0 },
  { "nullkeys", NDB_OPT_NOSHORT, "Pct nulls in each key attribute (default 10)",
    (uchar **)&g_opts.nullkeys, (uchar **)&g_opts.nullkeys, 0,
    GET_UINT, REQUIRED_ARG, 10, 0, 0, 0, 0, 0 },
  { "rpk", NDB_OPT_NOSHORT, "Avg records per full key (default 10)",
    (uchar **)&g_opts.rpk, (uchar **)&g_opts.rpk, 0,
    GET_UINT, REQUIRED_ARG, 10, 0, 0, 0, 0, 0 },
  { "rpkvar", NDB_OPT_NOSHORT, "Vary rpk by factor (default 10, none 1)",
    (uchar **)&g_opts.rpkvar, (uchar **)&g_opts.rpkvar, 0,
    GET_UINT, REQUIRED_ARG, 10, 0, 0, 0, 0, 0 },
  { "scanpct", NDB_OPT_NOSHORT,
    "Preferred max pct of total rows per scan (default 10)",
    (uchar **)&g_opts.scanpct, (uchar **)&g_opts.scanpct, 0,
    GET_UINT, REQUIRED_ARG, 5, 0, 0, 0, 0, 0 },
  { "eqscans", NDB_OPT_NOSHORT,
    "Pct scans for partial/full equality (default 30)",
    (uchar **)&g_opts.eqscans, (uchar **)&g_opts.eqscans, 0,
    GET_UINT, REQUIRED_ARG, 50, 0, 0, 0, 0, 0 },
  { "keeptable", NDB_OPT_NOSHORT,
    "Do not drop table at exit",
    (uchar **)&g_opts.keeptable, (uchar **)&g_opts.keeptable, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "abort", NDB_OPT_NOSHORT, "Dump core on any error",
    (uchar **)&g_opts.abort, (uchar **)&g_opts.abort, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "dump", NDB_OPT_NOSHORT, "Write CSV files name.* of keys,ranges,stats",
    (uchar **)&g_opts.dump, (uchar **)&g_opts.dump, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0,
    0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

static void
short_usage_sub()
{
  ndb_short_usage_sub(NULL);
}

static void
usage()
{
  ndbout << my_progname << ": ordered index stats test" << endl;
}

static int
checkoptions()
{
  chkrc(g_opts.rows != 0);
  chkrc(g_opts.nullkeys <= 100);
  chkrc(g_opts.rpk != 0);
  g_opts.rpk = min(g_opts.rpk, g_opts.rows);
  chkrc(g_opts.rpkvar != 0);
  chkrc(g_opts.scanpct <= 100);
  chkrc(g_opts.eqscans <= 100);
  // set value limits
  g_lim_val.all_nullable = false;
  g_lim_bnd.all_nullable = true;
  g_lim_val.b_min = g_opts.rows;
  g_lim_val.b_max = 2 * g_opts.rows;
  g_lim_bnd.b_min = 90 * g_lim_val.b_min / 100;
  g_lim_bnd.b_max = 110 * g_lim_val.b_max / 100;
  g_lim_val.c_char = "bcd";
  g_lim_bnd.c_char = "abcde";
  g_lim_val.d_min = 100;
  g_lim_val.d_max = 200;
  g_lim_bnd.d_min = 0;
  g_lim_bnd.d_max = 300;
  return 0;
}

static
int
docreate_stat_tables()
{
  if (g_is->check_systables(g_ndb_sys) == 0)
    return 0;
  ll1("check_systables: " << g_is->getNdbError());

  ll0("create stat tables");
  chkdb(g_is->create_systables(g_ndb_sys) == 0);
  g_has_created_stat_tables = true;
  return 0;
}

static
int
dodrop_stat_tables()
{
  if (g_has_created_stat_tables == false)
    return 0;

  ll0("drop stat tables");
  chkdb(g_is->drop_systables(g_ndb_sys) == 0);
  return 0;
}

static int
docreate_stat_events()
{
  if (g_is->check_sysevents(g_ndb_sys) == 0)
    return 0;
  ll1("check_sysevents: " << g_is->getNdbError());

  ll0("create stat events");
  chkdb(g_is->create_sysevents(g_ndb_sys) == 0);
  g_has_created_stat_events = true;
  return 0;
}

static int
dodrop_stat_events()
{
  if (g_has_created_stat_events == false)
    return 0;

  ll0("drop stat events");
  chkdb(g_is->drop_sysevents(g_ndb_sys) == 0);
  return 0;
}

static int
docreate_sys_objects()
{
  require(g_is != 0 && g_ndb_sys != 0);
  chkrc(docreate_stat_tables() == 0);
  chkrc(docreate_stat_events() == 0);
  return 0;
}

static int
dodrop_sys_objects()
{
  require(g_is != 0 && g_ndb_sys != 0);
  chkrc(dodrop_stat_events() == 0);
  chkrc(dodrop_stat_tables() == 0);
  return 0;
}

int
main(int argc, char** argv)
{
  ndb_init();
  my_progname = strchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
  uint i;
  ndbout << my_progname;
  for (i = 1; i < (uint)argc; i++)
    ndbout << " " << argv[i];
  ndbout << endl;
  int ret;
  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  ret = handle_options(&argc, &argv, my_long_options, ndb_std_get_one_option);
  if (ret != 0 || argc != 0) {
    ll0("wrong args");
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  if (checkoptions() == -1) {
    ll0("invalid args");
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  if (doconnect() == -1) {
    ll0("connect failed");
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  if (docreate_sys_objects() == -1) {
    ll0("failed to check or create stat tables and events");
    goto failed;
  }
  if (runtest() == -1) {
    ll0("test failed");
    goto failed;
  }
  if (dodrop_sys_objects() == -1) {
    ll0("failed to drop created stat tables or events");
    goto failed;
  }
  dodisconnect();
  return NDBT_ProgramExit(NDBT_OK);
failed:
  (void)dodrop_sys_objects();
  dodisconnect();
  return NDBT_ProgramExit(NDBT_FAILED);
}
