/* Copyright (C) 2003 MySQL AB

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

/*
 * testBlobs
 */

#include <ndb_global.h>
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbTest.hpp>
#include <NdbTick.h>

struct Bcol {
  bool m_nullable;
  unsigned m_inline;
  unsigned m_partsize;
  unsigned m_stripe;
  char m_btname[200];
  Bcol(bool a, unsigned b, unsigned c, unsigned d) :
    m_nullable(a),
    m_inline(b),
    m_partsize(c),
    m_stripe(d)
    {}
};

struct Opt {
  unsigned m_batch;
  bool m_core;
  bool m_dbg;
  bool m_dbgall;
  const char* m_dbug;
  bool m_fac;
  bool m_full;
  unsigned m_loop;
  unsigned m_parts;
  unsigned m_rows;
  unsigned m_seed;
  const char* m_skip;
  const char* m_test;
  // metadata
  const char* m_tname;
  const char* m_x1name;  // hash index
  const char* m_x2name;  // ordered index
  unsigned m_pk1off;
  unsigned m_pk2len;
  bool m_oneblob;
  Bcol m_blob1;
  Bcol m_blob2;
  // perf
  const char* m_tnameperf;
  unsigned m_rowsperf;
  // bugs
  int m_bug;
  int (*m_bugtest)();
  Opt() :
    m_batch(7),
    m_core(false),
    m_dbg(false),
    m_dbgall(false),
    m_dbug(0),
    m_fac(false),
    m_full(false),
    m_loop(1),
    m_parts(10),
    m_rows(100),
    m_seed(0),
    m_skip(0),
    m_test(0),
    // metadata
    m_tname("TBLOB1"),
    m_x1name("TBLOB1X1"),
    m_x2name("TBLOB1X2"),
    m_pk1off(0x12340000),
    m_pk2len(55),
    m_oneblob(false),
    m_blob1(false, 7, 1137, 10),
    m_blob2(true, 99, 55, 1),
    // perf
    m_tnameperf("TBLOB2"),
    m_rowsperf(10000),
    // bugs
    m_bug(0),
    m_bugtest(0) {
  }
};

static const unsigned g_max_pk2len = 256;

static void
printusage()
{
  Opt d;
  ndbout
    << "usage: testBlobs options [default/max]" << endl
    << "  -batch N    number of pk ops in batch [" << d.m_batch << "]" << endl
    << "  -core       dump core on error" << endl
    << "  -dbg        print debug" << endl
    << "  -dbgall     print also NDB API debug (if compiled in)" << endl
    << "  -dbug opt   dbug options" << endl
    << "  -fac        fetch across commit in scan delete [" << d.m_fac << "]" << endl
    << "  -full       read/write only full blob values" << endl
    << "  -loop N     loop N times 0=forever [" << d.m_loop << "]" << endl
    << "  -parts N    max parts in blob value [" << d.m_parts << "]" << endl
    << "  -rows N     number of rows [" << d.m_rows << "]" << endl
    << "  -rowsperf N rows for performace test [" << d.m_rowsperf << "]" << endl
    << "  -seed N     random seed 0=loop number [" << d.m_seed << "]" << endl
    << "  -skip xxx   skip given tests (see list) [no tests]" << endl
    << "  -test xxx   only given tests (see list) [all tests]" << endl
    << "metadata" << endl
    << "  -pk2len N   length of PK2 [" << d.m_pk2len << "/" << g_max_pk2len <<"]" << endl
    << "  -oneblob    only 1 blob attribute [default 2]" << endl
    << "test cases for test/skip" << endl
    << "  k           primary key ops" << endl
    << "  i           hash index ops" << endl
    << "  s           table scans" << endl
    << "  r           ordered index scans" << endl
    << "  p           performance test" << endl
    << "operations for test/skip" << endl
    << "  u           update existing blob value" << endl
    << "  n           normal insert and update" << endl
    << "  w           insert and update using writeTuple" << endl
    << "blob operation styles for test/skip" << endl
    << "  0           getValue / setValue" << endl
    << "  1           setActiveHook" << endl
    << "  2           readData / writeData" << endl
    << "example: -test kn0 (need all 3 parts)" << endl
    << "bug tests" << endl
    << "  -bug 4088   ndb api hang with mixed ops on index table" << endl
    << "  -bug 27018  middle partial part write clobbers rest of part" << endl
    << "  -bug 27370  Potential inconsistent blob reads for ReadCommitted reads" << endl
    ;
}

static Opt g_opt;

static bool
testcase(char x)
{
  if (x < 10)
    x += '0';
  return
    (g_opt.m_test == 0 || strchr(g_opt.m_test, x) != 0) &&
    (g_opt.m_skip == 0 || strchr(g_opt.m_skip, x) == 0);
}

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static NdbDictionary::Dictionary* g_dic = 0;
static NdbConnection* g_con = 0;
static NdbOperation* g_opr = 0;
static NdbIndexOperation* g_opx = 0;
static NdbScanOperation* g_ops = 0;
static NdbBlob* g_bh1 = 0;
static NdbBlob* g_bh2 = 0;
static bool g_printerror = true;
static unsigned g_loop = 0;

static void
printerror(int line, const char* msg)
{
  ndbout << "line " << line << " FAIL " << msg << endl;
  if (! g_printerror) {
    return;
  }
  if (g_ndb != 0 && g_ndb->getNdbError().code != 0) {
    ndbout << "ndb: " << g_ndb->getNdbError() << endl;
  }
  if (g_dic != 0 && g_dic->getNdbError().code != 0) {
    ndbout << "dic: " << g_dic->getNdbError() << endl;
  }
  if (g_con != 0 && g_con->getNdbError().code != 0) {
    ndbout << "con: " << g_con->getNdbError() << endl;
    if (g_opr != 0 && g_opr->getNdbError().code != 0) {
      ndbout << "opr: table=" << g_opr->getTableName() << " " << g_opr->getNdbError() << endl;
    }
    if (g_opx != 0 && g_opx->getNdbError().code != 0) {
      ndbout << "opx: table=" << g_opx->getTableName() << " " << g_opx->getNdbError() << endl;
    }
    if (g_ops != 0 && g_ops->getNdbError().code != 0) {
      ndbout << "ops: table=" << g_ops->getTableName() << " " << g_ops->getNdbError() << endl;
    }
    NdbOperation* ope = g_con->getNdbErrorOperation();
    if (ope != 0 && ope->getNdbError().code != 0) {
      if (ope != g_opr && ope != g_opx && ope != g_ops)
        ndbout << "ope: table=" << ope->getTableName() << " " << ope->getNdbError() << endl;
    }
  }
  if (g_bh1 != 0 && g_bh1->getNdbError().code != 0) {
    ndbout << "bh1: " << g_bh1->getNdbError() << endl;
  }
  if (g_bh2 != 0 && g_bh2->getNdbError().code != 0) {
    ndbout << "bh2: " << g_bh2->getNdbError() << endl;
  }
  if (g_opt.m_core) {
    abort();
  }
  g_printerror = false;
}

#define CHK(x) \
  do { \
    if (x) break; \
    printerror(__LINE__, #x); return -1; \
  } while (0)
#define DBG(x) \
  do { \
    if (! g_opt.m_dbg) break; \
    ndbout << "line " << __LINE__ << " " << x << endl; \
  } while (0)

static int
dropTable()
{
  NdbDictionary::Table tab(g_opt.m_tname);
  if (g_dic->getTable(g_opt.m_tname) != 0)
    CHK(g_dic->dropTable(g_opt.m_tname) == 0);
  return 0;
}

static int
createTable()
{
  NdbDictionary::Table tab(g_opt.m_tname);
  tab.setLogging(false);
  // col PK1 - Uint32
  { NdbDictionary::Column col("PK1");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  // col BL1 - Blob not-nullable
  { NdbDictionary::Column col("BL1");
    const Bcol& b = g_opt.m_blob1;
    col.setType(NdbDictionary::Column::Blob);
    col.setInlineSize(b.m_inline);
    col.setPartSize(b.m_partsize);
    col.setStripeSize(b.m_stripe);
    tab.addColumn(col);
  }
  // col PK2 - Char[55]
  if (g_opt.m_pk2len != 0)
  { NdbDictionary::Column col("PK2");
    col.setType(NdbDictionary::Column::Char);
    col.setLength(g_opt.m_pk2len);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  // col BL2 - Text nullable
  if (! g_opt.m_oneblob)
  { NdbDictionary::Column col("BL2");
    const Bcol& b = g_opt.m_blob2;
    col.setType(NdbDictionary::Column::Text);
    col.setNullable(true);
    col.setInlineSize(b.m_inline);
    col.setPartSize(b.m_partsize);
    col.setStripeSize(b.m_stripe);
    tab.addColumn(col);
  }
  // create table
  CHK(g_dic->createTable(tab) == 0);
  // unique hash index on PK2
  if (g_opt.m_pk2len != 0)
  { NdbDictionary::Index idx(g_opt.m_x1name);
    idx.setType(NdbDictionary::Index::UniqueHashIndex);
    idx.setLogging(false);
    idx.setTable(g_opt.m_tname);
    idx.addColumnName("PK2");
    CHK(g_dic->createIndex(idx) == 0);
  }
  // ordered index on PK2
  if (g_opt.m_pk2len != 0)
  { NdbDictionary::Index idx(g_opt.m_x2name);
    idx.setType(NdbDictionary::Index::OrderedIndex);
    idx.setLogging(false);
    idx.setTable(g_opt.m_tname);
    idx.addColumnName("PK2");
    CHK(g_dic->createIndex(idx) == 0);
  }
  return 0;
}

// tuples

struct Bval {
  char* m_val;
  unsigned m_len;
  char* m_buf; // read/write buffer
  unsigned m_buflen;
  int m_error_code; // for testing expected error code
  Bval() :
    m_val(0),
    m_len(0),
    m_buf(0),
    m_buflen(0),
    m_error_code(0)
    {}
  ~Bval() { delete [] m_val; delete [] m_buf; }
  void alloc(unsigned buflen) {
    m_buflen = buflen;
    delete [] m_buf;
    m_buf = new char [m_buflen];
    trash();
  }
  void copyfrom(const Bval& v) {
    m_len = v.m_len;
    delete [] m_val;
    if (v.m_val == 0)
      m_val = 0;
    else
      m_val = (char*)memcpy(new char [m_len], v.m_val, m_len);
  }
  void trash() const {
    assert(m_buf != 0);
    memset(m_buf, 'x', m_buflen);
  }
private:
  Bval(const Bval&);
  Bval& operator=(const Bval&);
};

struct Tup {
  bool m_exists;        // exists in table
  Uint32 m_pk1;         // primary keys concatenated like keyinfo
  char m_pk2[g_max_pk2len + 1];
  Bval m_blob1;
  Bval m_blob2;
  Tup() :
    m_exists(false)
    {}
  ~Tup() { }
  // alloc buffers of max size
  void alloc() {
    m_blob1.alloc(g_opt.m_blob1.m_inline + g_opt.m_blob1.m_partsize * g_opt.m_parts);
    m_blob2.alloc(g_opt.m_blob2.m_inline + g_opt.m_blob2.m_partsize * g_opt.m_parts);
  }
  void copyfrom(const Tup& tup) {
    assert(m_pk1 == tup.m_pk1);
    m_blob1.copyfrom(tup.m_blob1);
    m_blob2.copyfrom(tup.m_blob2);
  }
private:
  Tup(const Tup&);
  Tup& operator=(const Tup&);
};

static Tup* g_tups;

static unsigned
urandom(unsigned n)
{
  return n == 0 ? 0 : random() % n;
}

static void
calcBval(const Bcol& b, Bval& v, bool keepsize)
{
  if (b.m_nullable && urandom(10) == 0) {
    v.m_len = 0;
    delete [] v.m_val;
    v.m_val = 0;
    v.m_buf = new char [1];
  } else {
    if (keepsize && v.m_val != 0)
      ;
    else if (urandom(10) == 0)
      v.m_len = urandom(b.m_inline);
    else
      v.m_len = urandom(b.m_inline + g_opt.m_parts * b.m_partsize + 1);
    delete [] v.m_val;
    v.m_val = new char [v.m_len + 1];
    for (unsigned i = 0; i < v.m_len; i++)
      v.m_val[i] = 'a' + urandom(25);
    v.m_val[v.m_len] = 0;
    v.m_buf = new char [v.m_len];
  }
  v.m_buflen = v.m_len;
  v.trash();
}

static void
calcBval(Tup& tup, bool keepsize)
{
  calcBval(g_opt.m_blob1, tup.m_blob1, keepsize);
  if (! g_opt.m_oneblob)
    calcBval(g_opt.m_blob2, tup.m_blob2, keepsize);
}

static void
calcTups(bool keepsize)
{
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    tup.m_pk1 = g_opt.m_pk1off + k;
    for (unsigned i = 0, n = k; i < g_opt.m_pk2len; i++) {
      if (n != 0) {
        tup.m_pk2[i] = '0' + n % 10;
        n = n / 10;
      } else {
        tup.m_pk2[i] = 'a' + i % 26;
      }
    }
    calcBval(tup, keepsize);
  }
}

// blob handle ops

static int
getBlobHandles(NdbOperation* opr)
{
  CHK((g_bh1 = opr->getBlobHandle("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((g_bh2 = opr->getBlobHandle("BL2")) != 0);
  return 0;
}

static int
getBlobHandles(NdbIndexOperation* opx)
{
  CHK((g_bh1 = opx->getBlobHandle("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((g_bh2 = opx->getBlobHandle("BL2")) != 0);
  return 0;
}

static int
getBlobHandles(NdbScanOperation* ops)
{
  CHK((g_bh1 = ops->getBlobHandle("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((g_bh2 = ops->getBlobHandle("BL2")) != 0);
  return 0;
}

static int
getBlobLength(NdbBlob* h, unsigned& len)
{
  Uint64 len2 = (unsigned)-1;
  CHK(h->getLength(len2) == 0);
  len = (unsigned)len2;
  assert(len == len2);
  bool isNull;
  CHK(h->getNull(isNull) == 0);
  DBG("getBlobLength " << h->getColumn()->getName() << " len=" << len << " null=" << isNull);
  return 0;
}

// setValue / getValue

static int
setBlobValue(NdbBlob* h, const Bval& v, int error_code = 0)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  DBG("setValue " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null);
  if (null) {
    CHK(h->setNull() == 0 || h->getNdbError().code == error_code);
    if (error_code)
      return 0;
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
    CHK(h->setValue(v.m_val, v.m_len) == 0 || h->getNdbError().code == error_code);
    if (error_code)
      return 0;
    CHK(h->getNull(isNull) == 0 && isNull == false);
    CHK(getBlobLength(h, len) == 0 && len == v.m_len);
  }
  return 0;
}

static int
setBlobValue(const Tup& tup, int error_code = 0)
{
  CHK(setBlobValue(g_bh1, tup.m_blob1, error_code) == 0);
  if (! g_opt.m_oneblob)
    CHK(setBlobValue(g_bh2, tup.m_blob2, error_code) == 0);
  return 0;
}

static int
getBlobValue(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  DBG("getValue " <<  h->getColumn()->getName() << " buflen=" << v.m_buflen);
  CHK(h->getValue(v.m_buf, v.m_buflen) == 0);
  return 0;
}

static int
getBlobValue(const Tup& tup)
{
  CHK(getBlobValue(g_bh1, tup.m_blob1) == 0);
  if (! g_opt.m_oneblob)
    CHK(getBlobValue(g_bh2, tup.m_blob2) == 0);
  return 0;
}

static int
verifyBlobValue(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  if (null) {
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
    isNull = true;
    CHK(h->getNull(isNull) == 0 && isNull == false);
    CHK(getBlobLength(h, len) == 0 && len == v.m_len);
    for (unsigned i = 0; i < v.m_len; i++)
      CHK(v.m_val[i] == v.m_buf[i]);
  }
  return 0;
}

static int
verifyBlobValue(const Tup& tup)
{
  CHK(verifyBlobValue(g_bh1, tup.m_blob1) == 0);
  if (! g_opt.m_oneblob)
    CHK(verifyBlobValue(g_bh2, tup.m_blob2) == 0);
  return 0;
}

// readData / writeData

static int
writeBlobData(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  DBG("write " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null);
  int error_code = v.m_error_code;
  if (null) {
    CHK(h->setNull() == 0 || h->getNdbError().code == error_code);
    if (error_code)
      return 0;
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
    CHK(h->truncate(v.m_len) == 0 || h->getNdbError().code == error_code);
    if (error_code)
      return 0;
    unsigned n = 0;
    do {
      unsigned m = g_opt.m_full ? v.m_len : urandom(v.m_len + 1);
      if (m > v.m_len - n)
        m = v.m_len - n;
      DBG("write pos=" << n << " cnt=" << m);
      CHK(h->writeData(v.m_val + n, m) == 0);
      n += m;
    } while (n < v.m_len);
    assert(n == v.m_len);
    isNull = true;
    CHK(h->getNull(isNull) == 0 && isNull == false);
    CHK(getBlobLength(h, len) == 0 && len == v.m_len);
  }
  return 0;
}

static int
writeBlobData(Tup& tup, int error_code = 0)
{
  tup.m_blob1.m_error_code = error_code;
  CHK(writeBlobData(g_bh1, tup.m_blob1) == 0);
  if (! g_opt.m_oneblob) {
    tup.m_blob2.m_error_code = error_code;
    CHK(writeBlobData(g_bh2, tup.m_blob2) == 0);
  }
  return 0;
}

static int
readBlobData(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  DBG("read " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null);
  if (null) {
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
    isNull = true;
    CHK(h->getNull(isNull) == 0 && isNull == false);
    CHK(getBlobLength(h, len) == 0 && len == v.m_len);
    v.trash();
    unsigned n = 0;
    while (n < v.m_len) {
      unsigned m = g_opt.m_full ? v.m_len : urandom(v.m_len + 1);
      if (m > v.m_len - n)
        m = v.m_len - n;
      DBG("read pos=" << n << " cnt=" << m);
      const unsigned m2 = m;
      CHK(h->readData(v.m_buf + n, m) == 0);
      CHK(m2 == m);
      n += m;
    }
    assert(n == v.m_len);
    // need to execute to see the data
    CHK(g_con->execute(NoCommit) == 0);
    for (unsigned i = 0; i < v.m_len; i++)
      CHK(v.m_val[i] == v.m_buf[i]);
  }
  return 0;
}

static int
readBlobData(const Tup& tup)
{
  CHK(readBlobData(g_bh1, tup.m_blob1) == 0);
  if (! g_opt.m_oneblob)
    CHK(readBlobData(g_bh2, tup.m_blob2) == 0);
  return 0;
}

// hooks

static NdbBlob::ActiveHook blobWriteHook;

static int
blobWriteHook(NdbBlob* h, void* arg)
{
  DBG("blobWriteHook");
  Bval& v = *(Bval*)arg;
  CHK(writeBlobData(h, v) == 0);
  return 0;
}

static int
setBlobWriteHook(NdbBlob* h, Bval& v, int error_code = 0)
{
  DBG("setBlobWriteHook");
  v.m_error_code = error_code;
  CHK(h->setActiveHook(blobWriteHook, &v) == 0);
  return 0;
}

static int
setBlobWriteHook(Tup& tup, int error_code = 0)
{
  CHK(setBlobWriteHook(g_bh1, tup.m_blob1, error_code) == 0);
  if (! g_opt.m_oneblob)
    CHK(setBlobWriteHook(g_bh2, tup.m_blob2, error_code) == 0);
  return 0;
}

static NdbBlob::ActiveHook blobReadHook;

// no PK yet to identify tuple so just read the value
static int
blobReadHook(NdbBlob* h, void* arg)
{
  DBG("blobReadHook");
  Bval& v = *(Bval*)arg;
  unsigned len;
  CHK(getBlobLength(h, len) == 0);
  v.alloc(len);
  Uint32 maxlen = 0xffffffff;
  CHK(h->readData(v.m_buf, maxlen) == 0);
  DBG("read " << maxlen << " bytes");
  CHK(len == maxlen);
  return 0;
}

static int
setBlobReadHook(NdbBlob* h, Bval& v)
{
  DBG("setBlobReadHook");
  CHK(h->setActiveHook(blobReadHook, &v) == 0);
  return 0;
}

static int
setBlobReadHook(Tup& tup)
{
  CHK(setBlobReadHook(g_bh1, tup.m_blob1) == 0);
  if (! g_opt.m_oneblob)
    CHK(setBlobReadHook(g_bh2, tup.m_blob2) == 0);
  return 0;
}

// verify blob data

static int
verifyHeadInline(const Bcol& c, const Bval& v, NdbRecAttr* ra)
{
  if (v.m_val == 0) {
    CHK(ra->isNULL() == 1);
  } else {
    CHK(ra->isNULL() == 0);
    const NdbBlob::Head* head = (const NdbBlob::Head*)ra->aRef();
    CHK(head->length == v.m_len);
    const char* data = (const char*)(head + 1);
    for (unsigned i = 0; i < head->length && i < c.m_inline; i++)
      CHK(data[i] == v.m_val[i]);
  }
  return 0;
}

static int
verifyHeadInline(const Tup& tup)
{
  DBG("verifyHeadInline pk1=" << hex << tup.m_pk1);
  CHK((g_con = g_ndb->startTransaction()) != 0);
  CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
  CHK(g_opr->readTuple() == 0);
  CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
  if (g_opt.m_pk2len != 0)
    CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
  NdbRecAttr* ra1;
  NdbRecAttr* ra2;
  CHK((ra1 = g_opr->getValue("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((ra2 = g_opr->getValue("BL2")) != 0);
  if (tup.m_exists) {
    CHK(g_con->execute(Commit, AbortOnError) == 0);
    DBG("verifyHeadInline BL1");
    CHK(verifyHeadInline(g_opt.m_blob1, tup.m_blob1, ra1) == 0);
    if (! g_opt.m_oneblob) {
      DBG("verifyHeadInline BL2");
      CHK(verifyHeadInline(g_opt.m_blob2, tup.m_blob2, ra2) == 0);
    }
  } else {
    CHK(g_con->execute(Commit, AbortOnError) == -1 && 
	g_con->getNdbError().code == 626);
  }
  g_ndb->closeTransaction(g_con);
  g_opr = 0;
  g_con = 0;
  return 0;
}

static int
verifyBlobTable(const Bcol& b, const Bval& v, Uint32 pk1, bool exists)
{
  DBG("verify " << b.m_btname << " pk1=" << hex << pk1);
  NdbRecAttr* ra_pk;
  NdbRecAttr* ra_part;
  NdbRecAttr* ra_data;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  CHK((g_ops = g_con->getNdbScanOperation(b.m_btname)) != 0);
  CHK(g_ops->readTuples() == 0);
  CHK((ra_pk = g_ops->getValue("PK")) != 0);
  CHK((ra_part = g_ops->getValue("PART")) != 0);
  CHK((ra_data = g_ops->getValue("DATA")) != 0);
  CHK(g_con->execute(NoCommit) == 0);
  unsigned partcount;
  if (! exists || v.m_len <= b.m_inline)
    partcount = 0;
  else
    partcount = (v.m_len - b.m_inline + b.m_partsize - 1) / b.m_partsize;
  char* seen = new char [partcount];
  memset(seen, 0, partcount);
  while (1) {
    int ret;
    CHK((ret = g_ops->nextResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    if (pk1 != ra_pk->u_32_value())
      continue;
    Uint32 part = ra_part->u_32_value();
    DBG("part " << part << " of " << partcount);
    const char* data = ra_data->aRef();
    CHK(part < partcount && ! seen[part]);
    seen[part] = 1;
    unsigned n = b.m_inline + part * b.m_partsize;
    assert(exists && v.m_val != 0 && n < v.m_len);
    unsigned m = v.m_len - n;
    if (m > b.m_partsize)
      m = b.m_partsize;
    CHK(memcmp(data, v.m_val + n, m) == 0);
  }
  for (unsigned i = 0; i < partcount; i++)
    CHK(seen[i] == 1);
  g_ndb->closeTransaction(g_con);
  g_ops = 0;
  g_con = 0;
  return 0;
}

static int
verifyBlobTable(const Tup& tup)
{
  CHK(verifyBlobTable(g_opt.m_blob1, tup.m_blob1, tup.m_pk1, tup.m_exists) == 0);
  if (! g_opt.m_oneblob)
    CHK(verifyBlobTable(g_opt.m_blob2, tup.m_blob2, tup.m_pk1, tup.m_exists) == 0);
  return 0;
}

static int
verifyBlob()
{
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    const Tup& tup = g_tups[k];
    DBG("verifyBlob pk1=" << hex << tup.m_pk1);
    CHK(verifyHeadInline(tup) == 0);
    CHK(verifyBlobTable(tup) == 0);
  }
  return 0;
}

// operations

static const char* stylename[3] = {
  "style=getValue/setValue",
  "style=setActiveHook",
  "style=readData/writeData"
};

// pk ops

static int
insertPk(int style)
{
  DBG("--- insertPk " << stylename[style] << " ---");
  unsigned n = 0;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("insertPk pk1=" << hex << tup.m_pk1);
    CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->insertTuple() == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK(getBlobHandles(g_opr) == 0);
    if (style == 0) {
      CHK(setBlobValue(tup) == 0);
    } else if (style == 1) {
      // non-nullable must be set
      CHK(g_bh1->setValue("", 0) == 0);
      CHK(setBlobWriteHook(tup) == 0);
    } else {
      // non-nullable must be set
      CHK(g_bh1->setValue("", 0) == 0);
      CHK(g_con->execute(NoCommit) == 0);
      CHK(writeBlobData(tup) == 0);
    }
    if (++n == g_opt.m_batch) {
      CHK(g_con->execute(Commit) == 0);
      g_ndb->closeTransaction(g_con);
      CHK((g_con = g_ndb->startTransaction()) != 0);
      n = 0;
    }
    g_opr = 0;
    tup.m_exists = true;
  }
  if (n != 0) {
    CHK(g_con->execute(Commit) == 0);
    n = 0;
  }
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  return 0;
}

static int
readPk(int style)
{
  DBG("--- readPk " << stylename[style] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("readPk pk1=" << hex << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
    if (urandom(2) == 0)
      CHK(g_opr->readTuple() == 0);
    else
      CHK(g_opr->readTuple(NdbOperation::LM_CommittedRead) == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK(getBlobHandles(g_opr) == 0);
    if (style == 0) {
      CHK(getBlobValue(tup) == 0);
    } else if (style == 1) {
      CHK(setBlobReadHook(tup) == 0);
    } else {
      CHK(g_con->execute(NoCommit) == 0);
      CHK(readBlobData(tup) == 0);
    }
    CHK(g_con->execute(Commit) == 0);
    // verify lock mode upgrade
    CHK(g_opr->getLockMode() == NdbOperation::LM_Read);
    if (style == 0 || style == 1) {
      CHK(verifyBlobValue(tup) == 0);
    }
    g_ndb->closeTransaction(g_con);
    g_opr = 0;
    g_con = 0;
  }
  return 0;
}

static int
updatePk(int style)
{
  DBG("--- updatePk " << stylename[style] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("updatePk pk1=" << hex << tup.m_pk1);
    while (1) {
      int mode = urandom(3);
      int error_code = mode == 0 ? 0 : 4275;
      CHK((g_con = g_ndb->startTransaction()) != 0);
      CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
      if (mode == 0) {
        DBG("using updateTuple");
        CHK(g_opr->updateTuple() == 0);
      } else if (mode == 1) {
        DBG("using readTuple exclusive");
        CHK(g_opr->readTuple(NdbOperation::LM_Exclusive) == 0);
      } else {
        DBG("using readTuple - will fail and retry");
        CHK(g_opr->readTuple() == 0);
      }
      CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
      if (g_opt.m_pk2len != 0)
        CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
      CHK(getBlobHandles(g_opr) == 0);
      if (style == 0) {
        CHK(setBlobValue(tup, error_code) == 0);
      } else if (style == 1) {
        CHK(setBlobWriteHook(tup, error_code) == 0);
      } else {
        CHK(g_con->execute(NoCommit) == 0);
        CHK(writeBlobData(tup, error_code) == 0);
      }
      if (error_code == 0) {
        CHK(g_con->execute(Commit) == 0);
        g_ndb->closeTransaction(g_con);
        break;
      }
      g_ndb->closeTransaction(g_con);
    }
    g_opr = 0;
    g_con = 0;
    tup.m_exists = true;
  }
  return 0;
}

static int
writePk(int style)
{
  DBG("--- writePk " << stylename[style] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("writePk pk1=" << hex << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->writeTuple() == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK(getBlobHandles(g_opr) == 0);
    if (style == 0) {
      CHK(setBlobValue(tup) == 0);
    } else if (style == 1) {
      // non-nullable must be set
      CHK(g_bh1->setValue("", 0) == 0);
      CHK(setBlobWriteHook(tup) == 0);
    } else {
      // non-nullable must be set
      CHK(g_bh1->setValue("", 0) == 0);
      CHK(g_con->execute(NoCommit) == 0);
      CHK(writeBlobData(tup) == 0);
    }
    CHK(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);
    g_opr = 0;
    g_con = 0;
    tup.m_exists = true;
  }
  return 0;
}

static int
deletePk()
{
  DBG("--- deletePk ---");
  unsigned n = 0;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("deletePk pk1=" << hex << tup.m_pk1);
    CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->deleteTuple() == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    if (++n == g_opt.m_batch) {
      CHK(g_con->execute(Commit) == 0);
      g_ndb->closeTransaction(g_con);
      CHK((g_con = g_ndb->startTransaction()) != 0);
      n = 0;
    }
    g_opr = 0;
    tup.m_exists = false;
  }
  if (n != 0) {
    CHK(g_con->execute(Commit) == 0);
    n = 0;
  }
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  return 0;
}

static int
deleteNoPk()
{
  DBG("--- deleteNoPk ---");
  Tup no_tup; // bug#24028
  no_tup.m_pk1 = 0xb1ffb1ff;
  sprintf(no_tup.m_pk2, "%-*.*s", g_opt.m_pk2len, g_opt.m_pk2len,  "b1ffb1ff");
  CHK((g_con = g_ndb->startTransaction()) != 0);
  Tup& tup =  no_tup;
  DBG("deletePk pk1=" << hex << tup.m_pk1);
  CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
  CHK(g_opr->deleteTuple() == 0);
  CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
  if (g_opt.m_pk2len != 0)
    CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
  CHK(g_con->execute(Commit) == -1); // fail
  // BUG: error should be on op but is on con now
  DBG("con: " << g_con->getNdbError());
  DBG("opr: " << g_opr->getNdbError());
  CHK(g_con->getNdbError().code == 626 || g_opr->getNdbError().code == 626);
  g_ndb->closeTransaction(g_con);
  g_opr = 0;
  g_con = 0;
  return 0;
}

// hash index ops

static int
readIdx(int style)
{
  DBG("--- readIdx " << stylename[style] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("readIdx pk1=" << hex << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
    if (urandom(2) == 0)
      CHK(g_opx->readTuple() == 0);
    else
      CHK(g_opx->readTuple(NdbOperation::LM_CommittedRead) == 0);
    CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
    CHK(getBlobHandles(g_opx) == 0);
    if (style == 0) {
      CHK(getBlobValue(tup) == 0);
    } else if (style == 1) {
      CHK(setBlobReadHook(tup) == 0);
    } else {
      CHK(g_con->execute(NoCommit) == 0);
      CHK(readBlobData(tup) == 0);
    }
    CHK(g_con->execute(Commit) == 0);
    // verify lock mode upgrade (already done by NdbIndexOperation)
    CHK(g_opx->getLockMode() == NdbOperation::LM_Read);
    if (style == 0 || style == 1) {
      CHK(verifyBlobValue(tup) == 0);
    }
    g_ndb->closeTransaction(g_con);
    g_opx = 0;
    g_con = 0;
  }
  return 0;
}

static int
updateIdx(int style)
{
  DBG("--- updateIdx " << stylename[style] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("updateIdx pk1=" << hex << tup.m_pk1);
    // skip 4275 testing
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK(g_opx->updateTuple() == 0);
    CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
    CHK(getBlobHandles(g_opx) == 0);
    if (style == 0) {
      CHK(setBlobValue(tup) == 0);
    } else if (style == 1) {
      CHK(setBlobWriteHook(tup) == 0);
    } else {
      CHK(g_con->execute(NoCommit) == 0);
      CHK(writeBlobData(tup) == 0);
    }
    CHK(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);
    g_opx = 0;
    g_con = 0;
    tup.m_exists = true;
  }
  return 0;
}

static int
writeIdx(int style)
{
  DBG("--- writeIdx " << stylename[style] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("writeIdx pk1=" << hex << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK(g_opx->writeTuple() == 0);
    CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
    CHK(getBlobHandles(g_opx) == 0);
    if (style == 0) {
      CHK(setBlobValue(tup) == 0);
    } else if (style == 1) {
      // non-nullable must be set
      CHK(g_bh1->setValue("", 0) == 0);
      CHK(setBlobWriteHook(tup) == 0);
    } else {
      // non-nullable must be set
      CHK(g_bh1->setValue("", 0) == 0);
      CHK(g_con->execute(NoCommit) == 0);
      CHK(writeBlobData(tup) == 0);
    }
    CHK(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);
    g_opx = 0;
    g_con = 0;
    tup.m_exists = true;
  }
  return 0;
}

static int
deleteIdx()
{
  DBG("--- deleteIdx ---");
  unsigned n = 0;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("deleteIdx pk1=" << hex << tup.m_pk1);
    CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK(g_opx->deleteTuple() == 0);
    CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
    if (++n == g_opt.m_batch) {
      CHK(g_con->execute(Commit) == 0);
      g_ndb->closeTransaction(g_con);
      CHK((g_con = g_ndb->startTransaction()) != 0);
      n = 0;
    }
    g_opx = 0;
    tup.m_exists = false;
  }
  if (n != 0) {
    CHK(g_con->execute(Commit) == 0);
    n = 0;
  }
  return 0;
}

// scan ops table and index

static int
readScan(int style, bool idx)
{
  DBG("--- " << "readScan" << (idx ? "Idx" : "") << " " << stylename[style] << " ---");
  Tup tup;
  tup.alloc();  // allocate buffers
  CHK((g_con = g_ndb->startTransaction()) != 0);
  if (! idx) {
    CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
  } else {
    CHK((g_ops = g_con->getNdbIndexScanOperation(g_opt.m_x2name, g_opt.m_tname)) != 0);
  }
  if (urandom(2) == 0)
    CHK(g_ops->readTuples(NdbOperation::LM_Read) == 0);
  else
    CHK(g_ops->readTuples(NdbOperation::LM_CommittedRead) == 0);
  CHK(g_ops->getValue("PK1", (char*)&tup.m_pk1) != 0);
  if (g_opt.m_pk2len != 0)
    CHK(g_ops->getValue("PK2", tup.m_pk2) != 0);
  CHK(getBlobHandles(g_ops) == 0);
  if (style == 0) {
    CHK(getBlobValue(tup) == 0);
  } else if (style == 1) {
    CHK(setBlobReadHook(tup) == 0);
  }
  CHK(g_con->execute(NoCommit) == 0);
  // verify lock mode upgrade
  CHK(g_ops->getLockMode() == NdbOperation::LM_Read);
  unsigned rows = 0;
  while (1) {
    int ret;
    tup.m_pk1 = (Uint32)-1;
    memset(tup.m_pk2, 'x', g_opt.m_pk2len);
    CHK((ret = g_ops->nextResult(true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    DBG("readScan" << (idx ? "Idx" : "") << " pk1=" << hex << tup.m_pk1);
    Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
    CHK(k < g_opt.m_rows && g_tups[k].m_exists);
    tup.copyfrom(g_tups[k]);
    if (style == 0) {
      CHK(verifyBlobValue(tup) == 0);
    } else if (style == 1) {
      // execute ops generated by callbacks, if any
      CHK(verifyBlobValue(tup) == 0);
    } else {
      CHK(readBlobData(tup) == 0);
    }
    rows++;
  }
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_ops = 0;
  CHK(g_opt.m_rows == rows);
  return 0;
}

static int
updateScan(int style, bool idx)
{
  DBG("--- " << "updateScan" << (idx ? "Idx" : "") << " " << stylename[style] << " ---");
  Tup tup;
  tup.alloc();  // allocate buffers
  CHK((g_con = g_ndb->startTransaction()) != 0);
  if (! idx) {
    CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
  } else {
    CHK((g_ops = g_con->getNdbIndexScanOperation(g_opt.m_x2name, g_opt.m_tname)) != 0);
  }
  CHK(g_ops->readTuples(NdbOperation::LM_Exclusive) == 0);
  CHK(g_ops->getValue("PK1", (char*)&tup.m_pk1) != 0);
  if (g_opt.m_pk2len != 0)
    CHK(g_ops->getValue("PK2", tup.m_pk2) != 0);
  CHK(g_con->execute(NoCommit) == 0);
  unsigned rows = 0;
  while (1) {
    int ret;
    tup.m_pk1 = (Uint32)-1;
    memset(tup.m_pk2, 'x', g_opt.m_pk2len);
    CHK((ret = g_ops->nextResult(true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    DBG("updateScan" << (idx ? "Idx" : "") << " pk1=" << hex << tup.m_pk1);
    Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
    CHK(k < g_opt.m_rows && g_tups[k].m_exists);
    // calculate new blob values
    calcBval(g_tups[k], false);
    tup.copyfrom(g_tups[k]);
    // cannot do 4275 testing, scan op error code controls execution
    CHK((g_opr = g_ops->updateCurrentTuple()) != 0);
    CHK(getBlobHandles(g_opr) == 0);
    if (style == 0) {
      CHK(setBlobValue(tup) == 0);
    } else if (style == 1) {
      CHK(setBlobWriteHook(tup) == 0);
    } else {
      CHK(g_con->execute(NoCommit) == 0);
      CHK(writeBlobData(tup) == 0);
    }
    CHK(g_con->execute(NoCommit) == 0);
    g_opr = 0;
    rows++;
  }
  CHK(g_con->execute(Commit) == 0);
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_ops = 0;
  CHK(g_opt.m_rows == rows);
  return 0;
}

static int
deleteScan(bool idx)
{
  DBG("--- " << "deleteScan" << (idx ? "Idx" : "") << " ---");
  Tup tup;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  if (! idx) {
    CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
  } else {
    CHK((g_ops = g_con->getNdbIndexScanOperation(g_opt.m_x2name, g_opt.m_tname)) != 0);
  }
  CHK(g_ops->readTuples(NdbOperation::LM_Exclusive) == 0);
  CHK(g_ops->getValue("PK1", (char*)&tup.m_pk1) != 0);
  if (g_opt.m_pk2len != 0)
    CHK(g_ops->getValue("PK2", tup.m_pk2) != 0);
  CHK(g_con->execute(NoCommit) == 0);
  unsigned rows = 0;
  unsigned n = 0;
  while (1) {
    int ret;
    tup.m_pk1 = (Uint32)-1;
    memset(tup.m_pk2, 'x', g_opt.m_pk2len);
    CHK((ret = g_ops->nextResult(true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    while (1) {
      DBG("deleteScan" << (idx ? "Idx" : "") << " pk1=" << hex << tup.m_pk1);
      Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
      CHK(k < g_opt.m_rows && g_tups[k].m_exists);
      g_tups[k].m_exists = false;
      CHK(g_ops->deleteCurrentTuple() == 0);
      rows++;
      tup.m_pk1 = (Uint32)-1;
      memset(tup.m_pk2, 'x', g_opt.m_pk2len);
      CHK((ret = g_ops->nextResult(false)) == 0 || ret == 1 || ret == 2);
      if (++n == g_opt.m_batch || ret == 2) {
        DBG("execute batch: n=" << n << " ret=" << ret);
        if (! g_opt.m_fac) {
          CHK(g_con->execute(NoCommit) == 0);
        } else {
          CHK(g_con->execute(Commit) == 0);
          CHK(g_con->restart() == 0);
        }
        n = 0;
      }
      if (ret == 2)
        break;
    }
  }
  CHK(g_con->execute(Commit) == 0);
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_ops = 0;
  CHK(g_opt.m_rows == rows);
  return 0;
}

// main

// from here on print always
#undef DBG
#define DBG(x) \
  do { \
    ndbout << "line " << __LINE__ << " " << x << endl; \
  } while (0)

static int
testmain()
{
  g_ndb = new Ndb(g_ncc, "TEST_DB");
  CHK(g_ndb->init(20) == 0);
  CHK(g_ndb->waitUntilReady() == 0);
  g_dic = g_ndb->getDictionary();
  g_tups = new Tup [g_opt.m_rows];
  CHK(dropTable() == 0);
  CHK(createTable() == 0);
  if (g_opt.m_bugtest != 0) {
    // test a general bug instead of blobs
    CHK((*g_opt.m_bugtest)() == 0);
    return 0;
  }
  Bcol& b1 = g_opt.m_blob1;
  CHK(NdbBlob::getBlobTableName(b1.m_btname, g_ndb, g_opt.m_tname, "BL1") == 0);
  DBG("BL1: inline=" << b1.m_inline << " part=" << b1.m_partsize << " table=" << b1.m_btname);
  if (! g_opt.m_oneblob) {
    Bcol& b2 = g_opt.m_blob2;
    CHK(NdbBlob::getBlobTableName(b2.m_btname, g_ndb, g_opt.m_tname, "BL2") == 0);
    DBG("BL2: inline=" << b2.m_inline << " part=" << b2.m_partsize << " table=" << b2.m_btname);
  }
  if (g_opt.m_seed != 0)
    srandom(g_opt.m_seed);
  for (g_loop = 0; g_opt.m_loop == 0 || g_loop < g_opt.m_loop; g_loop++) {
    int style;
    DBG("=== loop " << g_loop << " ===");
    if (g_opt.m_seed == 0)
      srandom(g_loop);
    // pk
    for (style = 0; style <= 2; style++) {
      if (! testcase('k') || ! testcase(style))
        continue;
      DBG("--- pk ops " << stylename[style] << " ---");
      if (testcase('n')) {
        calcTups(false);
        CHK(insertPk(style) == 0);
        CHK(verifyBlob() == 0);
        CHK(readPk(style) == 0);
        if (testcase('u')) {
          calcTups(style);
          CHK(updatePk(style) == 0);
          CHK(verifyBlob() == 0);
          CHK(readPk(style) == 0);
        }
        CHK(deletePk() == 0);
        CHK(deleteNoPk() == 0);
        CHK(verifyBlob() == 0);
      }
      if (testcase('w')) {
        calcTups(false);
        CHK(writePk(style) == 0);
        CHK(verifyBlob() == 0);
        CHK(readPk(style) == 0);
        if (testcase('u')) {
          calcTups(style);
          CHK(writePk(style) == 0);
          CHK(verifyBlob() == 0);
          CHK(readPk(style) == 0);
        }
        CHK(deletePk() == 0);
        CHK(deleteNoPk() == 0);
        CHK(verifyBlob() == 0);
      }
    }
    // hash index
    for (style = 0; style <= 2; style++) {
      if (! testcase('i') || ! testcase(style))
        continue;
      DBG("--- idx ops " << stylename[style] << " ---");
      if (testcase('n')) {
        calcTups(false);
        CHK(insertPk(style) == 0);
        CHK(verifyBlob() == 0);
        CHK(readIdx(style) == 0);
        if (testcase('u')) {
          calcTups(style);
          CHK(updateIdx(style) == 0);
          CHK(verifyBlob() == 0);
          CHK(readIdx(style) == 0);
        }
        CHK(deleteIdx() == 0);
        CHK(verifyBlob() == 0);
      }
      if (testcase('w')) {
        calcTups(false);
        CHK(writePk(style) == 0);
        CHK(verifyBlob() == 0);
        CHK(readIdx(style) == 0);
        if (testcase('u')) {
          calcTups(style);
          CHK(writeIdx(style) == 0);
          CHK(verifyBlob() == 0);
          CHK(readIdx(style) == 0);
        }
        CHK(deleteIdx() == 0);
        CHK(verifyBlob() == 0);
      }
    }
    // scan table
    for (style = 0; style <= 2; style++) {
      if (! testcase('s') || ! testcase(style))
        continue;
      DBG("--- table scan " << stylename[style] << " ---");
      calcTups(false);
      CHK(insertPk(style) == 0);
      CHK(verifyBlob() == 0);
      CHK(readScan(style, false) == 0);
      if (testcase('u')) {
        CHK(updateScan(style, false) == 0);
        CHK(verifyBlob() == 0);
      }
      CHK(deleteScan(false) == 0);
      CHK(verifyBlob() == 0);
    }
    // scan index
    for (style = 0; style <= 2; style++) {
      if (! testcase('r') || ! testcase(style))
        continue;
      DBG("--- index scan " << stylename[style] << " ---");
      calcTups(false);
      CHK(insertPk(style) == 0);
      CHK(verifyBlob() == 0);
      CHK(readScan(style, true) == 0);
      if (testcase('u')) {
        CHK(updateScan(style, true) == 0);
        CHK(verifyBlob() == 0);
      }
      CHK(deleteScan(true) == 0);
      CHK(verifyBlob() == 0);
    }
  }
  delete g_ndb;
  return 0;
}

// separate performance test

struct Tmr {    // stolen from testOIBasic
  Tmr() {
    clr();
  }
  void clr() {
    m_on = m_ms = m_cnt = m_time[0] = m_text[0] = 0;
  }
  void on() {
    assert(m_on == 0);
    m_on = NdbTick_CurrentMillisecond();
  }
  void off(unsigned cnt = 0) {
    NDB_TICKS off = NdbTick_CurrentMillisecond();
    assert(m_on != 0 && off >= m_on);
    m_ms += off - m_on;
    m_cnt += cnt;
    m_on = 0;
  }
  const char* time() {
    if (m_cnt == 0)
      sprintf(m_time, "%u ms", m_ms);
    else
      sprintf(m_time, "%u ms per %u ( %u ms per 1000 )", m_ms, m_cnt, (1000 * m_ms) / m_cnt);
    return m_time;
  }
  const char* pct (const Tmr& t1) {
    if (0 < t1.m_ms)
      sprintf(m_text, "%u pct", (100 * m_ms) / t1.m_ms);
    else
      sprintf(m_text, "[cannot measure]");
    return m_text;
  }
  const char* over(const Tmr& t1) {
    if (0 < t1.m_ms) {
      if (t1.m_ms <= m_ms)
        sprintf(m_text, "%u pct", (100 * (m_ms - t1.m_ms)) / t1.m_ms);
      else
        sprintf(m_text, "-%u pct", (100 * (t1.m_ms - m_ms)) / t1.m_ms);
    } else
      sprintf(m_text, "[cannot measure]");
    return m_text;
  }
  NDB_TICKS m_on;
  unsigned m_ms;
  unsigned m_cnt;
  char m_time[100];
  char m_text[100];
};

static int
testperf()
{
  if (! testcase('p'))
    return 0;
  DBG("=== perf test ===");
  g_bh1 = g_bh2 = 0;
  g_ndb = new Ndb(g_ncc, "TEST_DB");
  CHK(g_ndb->init() == 0);
  CHK(g_ndb->waitUntilReady() == 0);
  g_dic = g_ndb->getDictionary();
  NdbDictionary::Table tab(g_opt.m_tnameperf);
  if (g_dic->getTable(tab.getName()) != 0)
    CHK(g_dic->dropTable(tab.getName()) == 0);
  // col A - pk
  { NdbDictionary::Column col("A");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  // col B - char 20
  { NdbDictionary::Column col("B");
    col.setType(NdbDictionary::Column::Char);
    col.setLength(20);
    col.setNullable(true);
    tab.addColumn(col);
  }
  // col C - text
  { NdbDictionary::Column col("C");
    col.setType(NdbDictionary::Column::Text);
    col.setInlineSize(20);
    col.setPartSize(512);
    col.setStripeSize(1);
    col.setNullable(true);
    tab.addColumn(col);
  }
  // create
  CHK(g_dic->createTable(tab) == 0);
  Uint32 cA = 0, cB = 1, cC = 2;
  // timers
  Tmr t1;
  Tmr t2;
  // insert char (one trans)
  {
    DBG("--- insert char ---");
    char b[20];
    t1.on();
    CHK((g_con = g_ndb->startTransaction()) != 0);
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->insertTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      memset(b, 0x20, sizeof(b));
      b[0] = 'b';
      CHK(g_opr->setValue(cB, b) == 0);
      CHK(g_con->execute(NoCommit) == 0);
    }
    t1.off(g_opt.m_rowsperf);
    CHK(g_con->execute(Rollback) == 0);
    DBG(t1.time());
    g_opr = 0;
    g_con = 0;
  }
  // insert text (one trans)
  {
    DBG("--- insert text ---");
    t2.on();
    CHK((g_con = g_ndb->startTransaction()) != 0);
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->insertTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      CHK((g_bh1 = g_opr->getBlobHandle(cC)) != 0);
      CHK((g_bh1->setValue("c", 1) == 0));
      CHK(g_con->execute(NoCommit) == 0);
    }
    t2.off(g_opt.m_rowsperf);
    CHK(g_con->execute(Rollback) == 0);
    DBG(t2.time());
    g_bh1 = 0;
    g_opr = 0;
    g_con = 0;
  }
  // insert overhead
  DBG("insert overhead: " << t2.over(t1));
  t1.clr();
  t2.clr();
  // insert
  {
    DBG("--- insert for read test ---");
    unsigned n = 0;
    char b[20];
    CHK((g_con = g_ndb->startTransaction()) != 0);
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->insertTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      memset(b, 0x20, sizeof(b));
      b[0] = 'b';
      CHK(g_opr->setValue(cB, b) == 0);
      CHK((g_bh1 = g_opr->getBlobHandle(cC)) != 0);
      CHK((g_bh1->setValue("c", 1) == 0));
      if (++n == g_opt.m_batch) {
        CHK(g_con->execute(Commit) == 0);
        g_ndb->closeTransaction(g_con);
        CHK((g_con = g_ndb->startTransaction()) != 0);
        n = 0;
      }
    }
    if (n != 0) {
      CHK(g_con->execute(Commit) == 0);
      g_ndb->closeTransaction(g_con); g_con = 0;
      n = 0;
    }
    g_bh1 = 0;
    g_opr = 0;
  }
  // pk read char (one trans)
  {
    DBG("--- pk read char ---");
    CHK((g_con = g_ndb->startTransaction()) != 0);
    Uint32 a;
    char b[20];
    t1.on();
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->readTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      CHK(g_opr->getValue(cA, (char*)&a) != 0);
      CHK(g_opr->getValue(cB, b) != 0);
      a = (Uint32)-1;
      b[0] = 0;
      CHK(g_con->execute(NoCommit) == 0);
      CHK(a == k && b[0] == 'b');
    }
    CHK(g_con->execute(Commit) == 0);
    t1.off(g_opt.m_rowsperf);
    DBG(t1.time());
    g_opr = 0;
    g_ndb->closeTransaction(g_con); g_con = 0;
  }
  // pk read text (one trans)
  {
    DBG("--- pk read text ---");
    CHK((g_con = g_ndb->startTransaction()) != 0);
    Uint32 a;
    char c[20];
    t2.on();
    for (Uint32 k = 0; k < g_opt.m_rowsperf; k++) {
      CHK((g_opr = g_con->getNdbOperation(tab.getName())) != 0);
      CHK(g_opr->readTuple() == 0);
      CHK(g_opr->equal(cA, (char*)&k) == 0);
      CHK(g_opr->getValue(cA, (char*)&a) != 0);
      CHK((g_bh1 = g_opr->getBlobHandle(cC)) != 0);
      a = (Uint32)-1;
      c[0] = 0;
      CHK(g_con->execute(NoCommit) == 0);
      Uint32 m = 20;
      CHK(g_bh1->readData(c, m) == 0);
      CHK(a == k && m == 1 && c[0] == 'c');
    }
    CHK(g_con->execute(Commit) == 0);
    t2.off(g_opt.m_rowsperf);
    DBG(t2.time());
    g_ndb->closeTransaction(g_con); g_opr = 0;
    g_con = 0;
  }
  // pk read overhead
  DBG("pk read overhead: " << t2.over(t1));
  t1.clr();
  t2.clr();
  // scan read char
  {
    DBG("--- scan read char ---");
    Uint32 a;
    char b[20];
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_ops = g_con->getNdbScanOperation(tab.getName())) != 0);
    CHK(g_ops->readTuples(NdbOperation::LM_Read) == 0);
    CHK(g_ops->getValue(cA, (char*)&a) != 0);
    CHK(g_ops->getValue(cB, b) != 0);
    CHK(g_con->execute(NoCommit) == 0);
    unsigned n = 0;
    t1.on();
    while (1) {
      a = (Uint32)-1;
      b[0] = 0;
      int ret;
      CHK((ret = g_ops->nextResult(true)) == 0 || ret == 1);
      if (ret == 1)
        break;
      CHK(a < g_opt.m_rowsperf && b[0] == 'b');
      n++;
    }
    CHK(n == g_opt.m_rowsperf);
    t1.off(g_opt.m_rowsperf);
    DBG(t1.time());
    g_ndb->closeTransaction(g_con); g_ops = 0;
    g_con = 0;
  }
  // scan read text
  {
    DBG("--- read text ---");
    Uint32 a;
    char c[20];
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_ops = g_con->getNdbScanOperation(tab.getName())) != 0);
    CHK(g_ops->readTuples(NdbOperation::LM_Read) == 0);
    CHK(g_ops->getValue(cA, (char*)&a) != 0);
    CHK((g_bh1 = g_ops->getBlobHandle(cC)) != 0);
    CHK(g_con->execute(NoCommit) == 0);
    unsigned n = 0;
    t2.on();
    while (1) {
      a = (Uint32)-1;
      c[0] = 0;
      int ret;
      CHK((ret = g_ops->nextResult(true)) == 0 || ret == 1);
      if (ret == 1)
        break;
      Uint32 m = 20;
      CHK(g_bh1->readData(c, m) == 0);
      CHK(a < g_opt.m_rowsperf && m == 1 && c[0] == 'c');
      n++;
    }
    CHK(n == g_opt.m_rowsperf);
    t2.off(g_opt.m_rowsperf);
    DBG(t2.time());
    g_bh1 = 0;
    g_ops = 0;
    g_ndb->closeTransaction(g_con); g_con = 0;
  }
  // scan read overhead
  DBG("scan read overhead: " << t2.over(t1));
  t1.clr();
  t2.clr();
  delete g_ndb;
  return 0;
}

// bug tests

static int
bugtest_4088()
{
  unsigned i;
  DBG("bug test 4088 - ndb api hang with mixed ops on index table");
  // insert rows
  calcTups(false);
  CHK(insertPk(false) == 0);
  // new trans
  CHK((g_con = g_ndb->startTransaction()) != 0);
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    // read table pk via index as a table
    const unsigned pkcnt = 2;
    Tup pktup[pkcnt];
    for (i = 0; i < pkcnt; i++) {
      char name[20];
      // XXX guess table id
      sprintf(name, "%d/%s", 4, g_opt.m_x1name);
      CHK((g_opr = g_con->getNdbOperation(name)) != 0);
      CHK(g_opr->readTuple() == 0);
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
      CHK(g_opr->getValue("NDB$PK", (char*)&pktup[i].m_pk1) != 0);
    }
    // read blob inline via index as an index
    CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK(g_opx->readTuple() == 0);
    CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
    assert(tup.m_blob1.m_buf != 0);
    CHK(g_opx->getValue("BL1", (char*)tup.m_blob1.m_buf) != 0);
    // execute
    // BUG 4088: gets 1 tckeyconf, 1 tcindxconf, then hangs
    CHK(g_con->execute(Commit) == 0);
    // verify
    for (i = 0; i < pkcnt; i++) {
      CHK(pktup[i].m_pk1 == tup.m_pk1);
      CHK(memcmp(pktup[i].m_pk2, tup.m_pk2, g_opt.m_pk2len) == 0);
    }
    CHK(memcmp(tup.m_blob1.m_val, tup.m_blob1.m_buf, 8 + g_opt.m_blob1.m_inline) == 0);
  }
  return 0;
}

static int
bugtest_27018()
{
  DBG("bug test 27018 - middle partial part write clobbers rest of part");

  // insert rows
  calcTups(false);
  CHK(insertPk(false) == 0);
  // new trans
  for (unsigned k= 0; k < g_opt.m_rows; k++)
  {
    Tup& tup= g_tups[k];

    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->updateTuple() == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK(getBlobHandles(g_opr) == 0);
    CHK(g_con->execute(NoCommit) == 0);

    /* Update one byte in random position. */
    Uint32 offset= urandom(tup.m_blob1.m_len);
    tup.m_blob1.m_buf[0]= 0xff ^ tup.m_blob1.m_val[offset];
    CHK(g_bh1->setPos(offset) == 0);
    CHK(g_bh1->writeData(&(tup.m_blob1.m_buf[0]), 1) == 0);
    CHK(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);

    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->readTuple() == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK(getBlobHandles(g_opr) == 0);

    CHK(g_bh1->getValue(tup.m_blob1.m_buf, tup.m_blob1.m_len) == 0);
    CHK(g_con->execute(Commit) == 0);
    Uint64 len= ~0;
    CHK(g_bh1->getLength(len) == 0 && len == tup.m_blob1.m_len);
    tup.m_blob1.m_buf[offset]^= 0xff;
    CHK(memcmp(tup.m_blob1.m_buf, tup.m_blob1.m_val, tup.m_blob1.m_len) == 0);
    g_ndb->closeTransaction(g_con);
  }

  return 0;
}


struct bug27370_data {
  Ndb *m_ndb;
  char m_current_write_value;
  char *m_writebuf;
  Uint32 m_blob1_size;
  Uint32 m_pk1;
  char m_pk2[g_max_pk2len + 1];
  bool m_thread_stop;
};

void *bugtest_27370_thread(void *arg)
{
  bug27370_data *data= (bug27370_data *)arg;

  while (!data->m_thread_stop)
  {
    memset(data->m_writebuf, data->m_current_write_value, data->m_blob1_size);
    data->m_current_write_value++;

    NdbConnection *con;
    if ((con= data->m_ndb->startTransaction()) == 0)
      return (void *)"Failed to create transaction";
    NdbOperation *opr;
    if ((opr= con->getNdbOperation(g_opt.m_tname)) == 0)
      return (void *)"Failed to create operation";
    if (opr->writeTuple() != 0)
      return (void *)"writeTuple() failed";
    if (opr->equal("PK1", data->m_pk1) != 0)
      return (void *)"equal(PK1) failed";
    if (g_opt.m_pk2len != 0)
      if (opr->equal("PK2", data->m_pk2) != 0)
        return (void *)"equal(PK2) failed";
    NdbBlob *bh;
    if ((bh= opr->getBlobHandle("BL1")) == 0)
      return (void *)"getBlobHandle() failed";
    if (bh->setValue(data->m_writebuf, data->m_blob1_size) != 0)
      return (void *)"setValue() failed";
    if (con->execute(Commit, AbortOnError, 1) != 0)
      return (void *)"execute() failed";
    data->m_ndb->closeTransaction(con);
  }

  return NULL;                                  // Success
}

static int
bugtest_27370()
{
  DBG("bug test 27370 - Potential inconsistent blob reads for ReadCommitted reads");

  bug27370_data data;

  data.m_ndb= new Ndb(g_ncc, "TEST_DB");
  CHK(data.m_ndb->init(20) == 0);
  CHK(data.m_ndb->waitUntilReady() == 0);

  data.m_current_write_value= 0;
  data.m_blob1_size= g_opt.m_blob1.m_inline + 10 * g_opt.m_blob1.m_partsize;
  CHK((data.m_writebuf= new char [data.m_blob1_size]) != 0);
  data.m_pk1= 27370;
  memset(data.m_pk2, 'x', g_max_pk2len);
  data.m_pk2[g_max_pk2len]= '\0';
  data.m_thread_stop= false;

  memset(data.m_writebuf, data.m_current_write_value, data.m_blob1_size);
  data.m_current_write_value++;

  CHK((g_con= g_ndb->startTransaction()) != 0);
  CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
  CHK(g_opr->writeTuple() == 0);
  CHK(g_opr->equal("PK1", data.m_pk1) == 0);
  if (g_opt.m_pk2len != 0)
    CHK(g_opr->equal("PK2", data.m_pk2) == 0);
  CHK((g_bh1= g_opr->getBlobHandle("BL1")) != 0);
  CHK(g_bh1->setValue(data.m_writebuf, data.m_blob1_size) == 0);
  CHK(g_con->execute(Commit) == 0);
  g_ndb->closeTransaction(g_con);
  g_con= NULL;

  pthread_t thread_handle;
  CHK(pthread_create(&thread_handle, NULL, bugtest_27370_thread, &data) == 0);

  DBG("bug test 27370 - PK blob reads");
  Uint32 seen_updates= 0;
  while (seen_updates < 50)
  {
    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_opr= g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->readTuple(NdbOperation::LM_CommittedRead) == 0);
    CHK(g_opr->equal("PK1", data.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", data.m_pk2) == 0);
    CHK((g_bh1= g_opr->getBlobHandle("BL1")) != 0);
    CHK(g_con->execute(NoCommit, AbortOnError, 1) == 0);

    const Uint32 loop_max= 10;
    char read_char;
    char original_read_char= 0;
    Uint32 readloop;
    for (readloop= 0;; readloop++)
    {
      if (readloop > 0)
      {
        if (readloop > 1)
        {
          /* Compare against first read. */
          CHK(read_char == original_read_char);
        }
        else
        {
          /*
            We count the number of times we see the other thread had the
            chance to update, so that we can be sure it had the opportunity
            to run a reasonable number of times before we stop.
          */
          if (original_read_char != read_char)
            seen_updates++;
          original_read_char= read_char;
        }
      }
      if (readloop > loop_max)
        break;
      Uint32 readSize= 1;
      CHK(g_bh1->setPos(urandom(data.m_blob1_size)) == 0);
      CHK(g_bh1->readData(&read_char, readSize) == 0);
      CHK(readSize == 1);
      ExecType commitType= readloop == loop_max ? Commit : NoCommit;
      CHK(g_con->execute(commitType, AbortOnError, 1) == 0);
    }
    g_ndb->closeTransaction(g_con);
    g_con= NULL;
  }

  DBG("bug test 27370 - table scan blob reads");
  seen_updates= 0;
  while (seen_updates < 50)
  {
    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_ops= g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
    CHK(g_ops->readTuples(NdbOperation::LM_CommittedRead) == 0);
    CHK((g_bh1= g_ops->getBlobHandle("BL1")) != 0);
    CHK(g_con->execute(NoCommit, AbortOnError, 1) == 0);
    CHK(g_ops->nextResult(true) == 0);

    const Uint32 loop_max= 10;
    char read_char;
    char original_read_char= 0;
    Uint32 readloop;
    for (readloop= 0;; readloop++)
    {
      if (readloop > 0)
      {
        if (readloop > 1)
        {
          /* Compare against first read. */
          CHK(read_char == original_read_char);
        }
        else
        {
          /*
            We count the number of times we see the other thread had the
            chance to update, so that we can be sure it had the opportunity
            to run a reasonable number of times before we stop.
          */
          if (original_read_char != read_char)
            seen_updates++;
          original_read_char= read_char;
        }
      }
      if (readloop > loop_max)
        break;
      Uint32 readSize= 1;
      CHK(g_bh1->setPos(urandom(data.m_blob1_size)) == 0);
      CHK(g_bh1->readData(&read_char, readSize) == 0);
      CHK(readSize == 1);
      CHK(g_con->execute(NoCommit, AbortOnError, 1) == 0);
    }

    CHK(g_ops->nextResult(true) == 1);
    g_ndb->closeTransaction(g_con);
    g_con= NULL;
  }

  data.m_thread_stop= true;
  void *thread_return;
  CHK(pthread_join(thread_handle, &thread_return) == 0);
  DBG("bug 27370 - thread return status: " <<
      (thread_return ? (char *)thread_return : "<null>"));
  CHK(thread_return == 0);

  g_con= NULL;
  g_opr= NULL;
  g_bh1= NULL;
  return 0;
}

static struct {
  int m_bug;
  int (*m_test)();
} g_bugtest[] = {
  { 4088, bugtest_4088 },
  { 27018, bugtest_27018 },
  { 27370, bugtest_27370 }
};

NDB_COMMAND(testOdbcDriver, "testBlobs", "testBlobs", "testBlobs", 65535)
{
  ndb_init();
  while (++argv, --argc > 0) {
    const char* arg = argv[0];
    if (strcmp(arg, "-batch") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_batch = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-core") == 0) {
      g_opt.m_core = true;
      continue;
    }
    if (strcmp(arg, "-dbg") == 0) {
      g_opt.m_dbg = true;
      continue;
    }
    if (strcmp(arg, "-dbgall") == 0) {
      g_opt.m_dbg = true;
      g_opt.m_dbgall = true;
      putenv(strdup("NDB_BLOB_DEBUG=1"));
      continue;
    }
    if (strcmp(arg, "-dbug") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_dbug = strdup(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-fac") == 0) {
      g_opt.m_fac = true;
      continue;
    }
    if (strcmp(arg, "-full") == 0) {
      g_opt.m_full = true;
      continue;
    }
    if (strcmp(arg, "-loop") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_loop = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-parts") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_parts = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-rows") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_rows = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-rowsperf") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_rowsperf = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-seed") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_seed = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-skip") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_skip = strdup(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-test") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_test = strdup(argv[0]);
	continue;
      }
    }
    // metadata
    if (strcmp(arg, "-pk2len") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_pk2len = atoi(argv[0]);
        if (g_opt.m_pk2len <= g_max_pk2len)
          continue;
      }
    }
    if (strcmp(arg, "-oneblob") == 0) {
      g_opt.m_oneblob = true;
      continue;
    }
    // bugs
    if (strcmp(arg, "-bug") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_bug = atoi(argv[0]);
        for (unsigned i = 0; i < sizeof(g_bugtest)/sizeof(g_bugtest[0]); i++) {
          if (g_opt.m_bug == g_bugtest[i].m_bug) {
            g_opt.m_bugtest = g_bugtest[i].m_test;
            break;
          }
        }
        if (g_opt.m_bugtest != 0)
          continue;
      }
    }
    ndbout << "testOIBasic: unknown option " << arg << endl;
    printusage();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  if (g_opt.m_dbug != 0) {
    DBUG_PUSH(g_opt.m_dbug);
  }
  if (g_opt.m_pk2len == 0) {
    char b[100];
    b[0] = 0;
    if (g_opt.m_skip != 0)
      strcpy(b, g_opt.m_skip);
    strcat(b, "i");
    strcat(b, "r");
    g_opt.m_skip = strdup(b);
  }
  g_ncc = new Ndb_cluster_connection();
  if (g_ncc->connect(30) != 0 || testmain() == -1 || testperf() == -1) {
    ndbout << "line " << __LINE__ << " FAIL loop=" << g_loop << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  delete g_ncc;
  g_ncc = 0;
  return NDBT_ProgramExit(NDBT_OK);
}

// vim: set sw=2 et:
