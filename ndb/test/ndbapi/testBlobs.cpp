/* Copyright (C) 2003 MySQL AB

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

/*
 * testBlobs
 */

#include <ndb_global.h>
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbTest.hpp>

struct Bcol {
  bool m_nullable;
  unsigned m_inline;
  unsigned m_partsize;
  unsigned m_stripe;
  char m_btname[NdbBlob::BlobTableNameSize];
  Bcol(bool a, unsigned b, unsigned c, unsigned d) :
    m_nullable(a),
    m_inline(b),
    m_partsize(c),
    m_stripe(d)
    {}
};

struct Opt {
  bool m_core;
  bool m_dbg;
  bool m_dbgall;
  bool m_full;
  unsigned m_loop;
  unsigned m_parts;
  unsigned m_rows;
  unsigned m_seed;
  char m_skip[255];
  // metadata
  const char* m_tname;
  const char* m_x1name;  // hash index
  const char* m_x2name;  // ordered index
  unsigned m_pk1off;
  unsigned m_pk2len;
  bool m_oneblob;
  Bcol m_blob1;
  Bcol m_blob2;
  // bugs
  int m_bug;
  int (*m_bugtest)();
  Opt() :
    m_core(false),
    m_dbg(false),
    m_dbgall(false),
    m_full(false),
    m_loop(1),
    m_parts(10),
    m_rows(100),
    m_seed(0),
    // metadata
    m_tname("TBLOB1"),
    m_x1name("TBLOB1X1"),
    m_x2name("TBLOB1X2"),
    m_pk1off(0x12340000),
    m_pk2len(55),
    m_oneblob(false),
    m_blob1(false, 7, 1137, 10),
    m_blob2(true, 99, 55, 1),
    // bugs
    m_bug(0),
    m_bugtest(0) {
    memset(m_skip, false, sizeof(m_skip));
  }
};

static const unsigned g_max_pk2len = 256;

static void
printusage()
{
  Opt d;
  ndbout
    << "usage: testBlobs options [default/max]" << endl
    << "  -core       dump core on error" << endl
    << "  -dbg        print debug" << endl
    << "  -dbgall     print also NDB API debug (if compiled in)" << endl
    << "  -full       read/write only full blob values" << endl
    << "  -inline     read/write only blobs which fit inline" << endl
    << "  -loop N     loop N times 0=forever [" << d.m_loop << "]" << endl
    << "  -parts N    max parts in blob value [" << d.m_parts << "]" << endl
    << "  -rows N     number of rows [" << d.m_rows << "]" << endl
    << "  -seed N     random seed 0=loop number [" << d.m_seed << "]" << endl
    << "  -skip xxx   skip these tests (see list)" << endl
    << "metadata" << endl
    << "  -pk2len N   length of PK2 [" << d.m_pk2len << "/" << g_max_pk2len <<"]" << endl
    << "  -oneblob    only 1 blob attribute [default 2]" << endl
    << "testcases for -skip" << endl
    << "  k           primary key ops" << endl
    << "  i           hash index ops" << endl
    << "  s           table scans" << endl
    << "  r           ordered index scans" << endl
    << "  u           update blob value" << endl
    << "  v           getValue / setValue" << endl
    << "  w           readData / writeData" << endl
    << "bug tests (no blob test)" << endl
    << "  -bug 4088   ndb api hang with mixed ops on index table" << endl
    << "  -bug 2222   delete + write gives 626" << endl
    << "  -bug 3333   acc crash on delete and long key" << endl
    ;
}

static Opt g_opt;

static char&
skip(unsigned x)
{
  assert(x < sizeof(g_opt.m_skip));
  return g_opt.m_skip[x];
}

static Ndb* g_ndb = 0;
static NdbDictionary::Dictionary* g_dic = 0;
static NdbConnection* g_con = 0;
static NdbOperation* g_opr = 0;
static NdbIndexOperation* g_opx = 0;
static NdbScanOperation* g_ops = 0;
static NdbBlob* g_bh1 = 0;
static NdbBlob* g_bh2 = 0;
static bool g_printerror = true;

static void
printerror(int line, const char* msg)
{
  ndbout << "line " << line << ": " << msg << " failed" << endl;
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
    CHK(g_dic->dropTable(tab) == 0);
  return 0;
}

static int
createTable()
{
  NdbDictionary::Table tab(g_opt.m_tname);
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
  // col BL2 - Clob nullable
  if (! g_opt.m_oneblob)
  { NdbDictionary::Column col("BL2");
    const Bcol& b = g_opt.m_blob2;
    col.setType(NdbDictionary::Column::Clob);
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
  char* m_buf;
  unsigned m_buflen;
  Bval() :
    m_val(0),
    m_len(0),
    m_buf(0),   // read/write buffer
    m_buflen(0)
    {}
  ~Bval() { delete [] m_val; delete [] m_buf; }
  void alloc(unsigned buflen) {
    m_buflen = buflen;
    delete [] m_buf;
    m_buf = new char [m_buflen];
    trash();
  }
  void copy(const Bval& v) {
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
  void copy(const Tup& tup) {
    assert(m_pk1 == tup.m_pk1);
    m_blob1.copy(tup.m_blob1);
    m_blob2.copy(tup.m_blob2);
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
    delete v.m_val;
    v.m_val = 0;
    v.m_buf = new char [1];
  } else {
    if (keepsize && v.m_val != 0)
      ;
    else if (urandom(10) == 0)
      v.m_len = urandom(b.m_inline);
    else
      v.m_len = urandom(b.m_inline + g_opt.m_parts * b.m_partsize + 1);
    delete v.m_val;
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
    calcBval(g_opt.m_blob1, tup.m_blob1, keepsize);
    if (! g_opt.m_oneblob)
      calcBval(g_opt.m_blob2, tup.m_blob2, keepsize);
  }
}

// blob handle ops

static int
getBlobLength(NdbBlob* h, unsigned& len)
{
  Uint64 len2 = (unsigned)-1;
  CHK(h->getLength(len2) == 0);
  len = (unsigned)len2;
  assert(len == len2);
  return 0;
}

static int
setBlobValue(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  DBG("set " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null);
  if (null) {
    CHK(h->setNull() == 0);
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
    CHK(h->setValue(v.m_val, v.m_len) == 0);
    CHK(h->getNull(isNull) == 0 && isNull == false);
    CHK(getBlobLength(h, len) == 0 && len == v.m_len);
  }
  return 0;
}

static int
getBlobValue(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  DBG("get " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null);
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

static int
writeBlobData(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  DBG("write " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null);
  if (null) {
    CHK(h->setNull() == 0);
    isNull = false;
    CHK(h->getNull(isNull) == 0 && isNull == true);
    CHK(getBlobLength(h, len) == 0 && len == 0);
  } else {
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

// verify blob data

static int
verifyHeadInline(const Bcol& c, const Bval& v, NdbRecAttr* ra)
{
  if (v.m_val == 0) {
    CHK(ra->isNULL() == 1);
  } else {
    CHK(ra->isNULL() == 0);
    CHK(ra->u_64_value() == v.m_len);
  }
  return 0;
}

static int
verifyHeadInline(const Tup& tup)
{
  DBG("verifyHeadInline pk1=" << tup.m_pk1);
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
    CHK(g_con->execute(Commit) == 0);
    DBG("verifyHeadInline BL1");
    CHK(verifyHeadInline(g_opt.m_blob1, tup.m_blob1, ra1) == 0);
    if (! g_opt.m_oneblob) {
      DBG("verifyHeadInline BL2");
      CHK(verifyHeadInline(g_opt.m_blob2, tup.m_blob2, ra2) == 0);
    }
  } else {
    CHK(g_con->execute(Commit) == -1 && g_con->getNdbError().code == 626);
  }
  g_ndb->closeTransaction(g_con);
  g_opr = 0;
  g_con = 0;
  return 0;
}

static int
verifyBlobTable(const Bcol& b, const Bval& v, Uint32 pk1, bool exists)
{
  DBG("verify " << b.m_btname << " pk1=" << pk1);
  NdbRecAttr* ra_pk;
  NdbRecAttr* ra_part;
  NdbRecAttr* ra_data;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  CHK((g_opr = g_con->getNdbOperation(b.m_btname)) != 0);
  CHK(g_opr->openScanRead() == 0);
  CHK((ra_pk = g_opr->getValue("PK")) != 0);
  CHK((ra_part = g_opr->getValue("PART")) != 0);
  CHK((ra_data = g_opr->getValue("DATA")) != 0);
  CHK(g_con->executeScan() == 0);
  unsigned partcount;
  if (! exists || v.m_len <= b.m_inline)
    partcount = 0;
  else
    partcount = (v.m_len - b.m_inline + b.m_partsize - 1) / b.m_partsize;
  char* seen = new char [partcount];
  memset(seen, 0, partcount);
  while (1) {
    int ret;
    CHK((ret = g_con->nextScanResult()) == 0 || ret == 1);
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
  g_opr = 0;
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
    DBG("verifyBlob pk1=" << tup.m_pk1);
    CHK(verifyHeadInline(tup) == 0);
    CHK(verifyBlobTable(tup) == 0);
  }
  return 0;
}

// operations

static int
insertPk(bool rw)
{
  DBG("--- insertPk ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("insertPk pk1=" << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->insertTuple() == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK((g_bh1 = g_opr->getBlobHandle("BL1")) != 0);
    if (! g_opt.m_oneblob)
      CHK((g_bh2 = g_opr->getBlobHandle("BL2")) != 0);
    if (! rw) {
      CHK(setBlobValue(g_bh1, tup.m_blob1) == 0);
      if (! g_opt.m_oneblob)
        CHK(setBlobValue(g_bh2, tup.m_blob2) == 0);
    } else {
      // non-nullable must be set
      CHK(g_bh1->setValue("", 0) == 0);
      CHK(g_con->execute(NoCommit) == 0);
      CHK(writeBlobData(g_bh1, tup.m_blob1) == 0);
      if (! g_opt.m_oneblob)
        CHK(writeBlobData(g_bh2, tup.m_blob2) == 0);
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
updatePk(bool rw)
{
  DBG("--- updatePk ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("updatePk pk1=" << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->updateTuple() == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK((g_bh1 = g_opr->getBlobHandle("BL1")) != 0);
    if (! g_opt.m_oneblob)
      CHK((g_bh2 = g_opr->getBlobHandle("BL2")) != 0);
    if (! rw) {
      CHK(setBlobValue(g_bh1, tup.m_blob1) == 0);
      if (! g_opt.m_oneblob)
        CHK(setBlobValue(g_bh2, tup.m_blob2) == 0);
    } else {
      CHK(g_con->execute(NoCommit) == 0);
      CHK(writeBlobData(g_bh1, tup.m_blob1) == 0);
      if (! g_opt.m_oneblob)
        CHK(writeBlobData(g_bh2, tup.m_blob2) == 0);
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
updateIdx(bool rw)
{
  DBG("--- updateIdx ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("updateIdx pk1=" << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK(g_opx->updateTuple() == 0);
    CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
    CHK((g_bh1 = g_opx->getBlobHandle("BL1")) != 0);
    if (! g_opt.m_oneblob)
      CHK((g_bh2 = g_opx->getBlobHandle("BL2")) != 0);
    if (! rw) {
      CHK(setBlobValue(g_bh1, tup.m_blob1) == 0);
      if (! g_opt.m_oneblob)
        CHK(setBlobValue(g_bh2, tup.m_blob2) == 0);
    } else {
      CHK(g_con->execute(NoCommit) == 0);
      CHK(writeBlobData(g_bh1, tup.m_blob1) == 0);
      if (! g_opt.m_oneblob)
        CHK(writeBlobData(g_bh2, tup.m_blob2) == 0);
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
readPk(bool rw)
{
  DBG("--- readPk ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("readPk pk1=" << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->readTuple() == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK((g_bh1 = g_opr->getBlobHandle("BL1")) != 0);
    if (! g_opt.m_oneblob)
      CHK((g_bh2 = g_opr->getBlobHandle("BL2")) != 0);
    if (! rw) {
      CHK(getBlobValue(tup) == 0);
    } else {
      CHK(g_con->execute(NoCommit) == 0);
      CHK(readBlobData(tup) == 0);
    }
    CHK(g_con->execute(Commit) == 0);
    if (! rw) {
      CHK(verifyBlobValue(tup) == 0);
    }
    g_ndb->closeTransaction(g_con);
    g_opr = 0;
    g_con = 0;
  }
  return 0;
}

static int
readIdx(bool rw)
{
  DBG("--- readIdx ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("readIdx pk1=" << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK(g_opx->readTuple() == 0);
    CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
    CHK((g_bh1 = g_opx->getBlobHandle("BL1")) != 0);
    if (! g_opt.m_oneblob)
      CHK((g_bh2 = g_opx->getBlobHandle("BL2")) != 0);
    if (! rw) {
      CHK(getBlobValue(tup) == 0);
    } else {
      CHK(g_con->execute(NoCommit) == 0);
      CHK(readBlobData(tup) == 0);
    }
    CHK(g_con->execute(Commit) == 0);
    if (! rw) {
      CHK(verifyBlobValue(tup) == 0);
    }
    g_ndb->closeTransaction(g_con);
    g_opx = 0;
    g_con = 0;
  }
  return 0;
}

static int
readScan(bool rw, bool idx)
{
  const char* func = ! idx ? "scan read table" : "scan read index";
  DBG("--- " << func << " ---");
  Tup tup;
  tup.alloc();  // allocate buffers
  NdbResultSet* rs;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  if (! idx) {
    CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
  } else {
    CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_x2name, g_opt.m_tname)) != 0);
  }
  CHK((rs = g_ops->readTuples(240, NdbScanOperation::LM_Exclusive)) != 0);
  CHK(g_ops->getValue("PK1", (char*)&tup.m_pk1) != 0);
  if (g_opt.m_pk2len != 0)
    CHK(g_ops->getValue("PK2", tup.m_pk2) != 0);
  CHK((g_bh1 = g_ops->getBlobHandle("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((g_bh2 = g_ops->getBlobHandle("BL2")) != 0);
  if (! rw) {
    CHK(getBlobValue(tup) == 0);
  }
  CHK(g_con->execute(NoCommit) == 0);
  unsigned rows = 0;
  while (1) {
    int ret;
    tup.m_pk1 = (Uint32)-1;
    memset(tup.m_pk2, 'x', g_opt.m_pk2len);
    CHK((ret = rs->nextResult(true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    DBG(func << " pk1=" << tup.m_pk1);
    Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
    CHK(k < g_opt.m_rows && g_tups[k].m_exists);
    tup.copy(g_tups[k]);
    if (! rw) {
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
deletePk()
{
  DBG("--- deletePk ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("deletePk pk1=" << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
    CHK(g_opr->deleteTuple() == 0);
    CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
    if (g_opt.m_pk2len != 0)
      CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);
    g_opr = 0;
    g_con = 0;
    tup.m_exists = false;
  }
  return 0;
}

static int
deleteIdx()
{
  DBG("--- deleteIdx ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("deleteIdx pk1=" << tup.m_pk1);
    CHK((g_con = g_ndb->startTransaction()) != 0);
    CHK((g_opx = g_con->getNdbIndexOperation(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK(g_opx->deleteTuple() == 0);
    CHK(g_opx->equal("PK2", tup.m_pk2) == 0);
    CHK(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);
    g_opx = 0;
    g_con = 0;
    tup.m_exists = false;
  }
  return 0;
}

static int
deleteScan(bool idx)
{
  const char* func = ! idx ? "scan delete table" : "scan delete index";
  DBG("--- " << func << " ---");
  Tup tup;
  NdbResultSet* rs;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  if (! idx) {
    CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_tname)) != 0);
  } else {
    CHK((g_ops = g_con->getNdbScanOperation(g_opt.m_x2name, g_opt.m_tname)) != 0);
  }
  CHK((rs = g_ops->readTuples(240, NdbScanOperation::LM_Exclusive)) != 0);
  CHK(g_ops->getValue("PK1", (char*)&tup.m_pk1) != 0);
  if (g_opt.m_pk2len != 0)
    CHK(g_ops->getValue("PK2", tup.m_pk2) != 0);
  CHK(g_con->execute(NoCommit) == 0);
  unsigned rows = 0;
  while (1) {
    int ret;
    tup.m_pk1 = (Uint32)-1;
    memset(tup.m_pk2, 'x', g_opt.m_pk2len);
    CHK((ret = rs->nextResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    DBG(func << " pk1=" << tup.m_pk1);
    CHK(rs->deleteTuple() == 0);
    CHK(g_con->execute(NoCommit) == 0);
    Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
    CHK(k < g_opt.m_rows && g_tups[k].m_exists);
    g_tups[k].m_exists = false;
    rows++;
  }
  CHK(g_con->execute(Commit) == 0);
  g_ndb->closeTransaction(g_con);
  g_con = 0;
  g_opr = 0;
  g_ops = 0;
  CHK(g_opt.m_rows == rows);
  return 0;
}

// main

static int
testmain()
{
  g_ndb = new Ndb("TEST_DB");
  CHK(g_ndb->init() == 0);
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
  for (unsigned loop = 0; g_opt.m_loop == 0 || loop < g_opt.m_loop; loop++) {
    DBG("=== loop " << loop << " ===");
    if (g_opt.m_seed == 0)
      srandom(loop);
    bool llim = skip('v') ? true : false;
    bool ulim = skip('w') ? false : true;
    // pk
    for (int rw = llim; rw <= ulim; rw++) {
      if (skip('k'))
        continue;
      DBG("--- pk ops " << (! rw ? "get/set" : "read/write") << " ---");
      calcTups(false);
      CHK(insertPk(rw) == 0);
      CHK(verifyBlob() == 0);
      CHK(readPk(rw) == 0);
      if (! skip('u')) {
        calcTups(rw);
        CHK(updatePk(rw) == 0);
        CHK(verifyBlob() == 0);
      }
      CHK(readPk(rw) == 0);
      CHK(deletePk() == 0);
      CHK(verifyBlob() == 0);
    }
    // hash index
    for (int rw = llim; rw <= ulim; rw++) {
      if (skip('i'))
        continue;
      DBG("--- idx ops " << (! rw ? "get/set" : "read/write") << " ---");
      calcTups(false);
      CHK(insertPk(rw) == 0);
      CHK(verifyBlob() == 0);
      CHK(readIdx(rw) == 0);
      calcTups(rw);
      if (! skip('u')) {
        CHK(updateIdx(rw) == 0);
        CHK(verifyBlob() == 0);
        CHK(readIdx(rw) == 0);
      }
      CHK(deleteIdx() == 0);
      CHK(verifyBlob() == 0);
    }
    // scan table
    for (int rw = llim; rw <= ulim; rw++) {
      if (skip('s'))
        continue;
      DBG("--- table scan " << (! rw ? "get/set" : "read/write") << " ---");
      calcTups(false);
      CHK(insertPk(rw) == 0);
      CHK(verifyBlob() == 0);
      CHK(readScan(rw, false) == 0);
      CHK(deleteScan(false) == 0);
      CHK(verifyBlob() == 0);
    }
    // scan index
    for (int rw = llim; rw <= ulim; rw++) {
      if (skip('r'))
        continue;
      DBG("--- index scan " << (! rw ? "get/set" : "read/write") << " ---");
      calcTups(false);
      CHK(insertPk(rw) == 0);
      CHK(verifyBlob() == 0);
      CHK(readScan(rw, true) == 0);
      CHK(deleteScan(true) == 0);
      CHK(verifyBlob() == 0);
    }
  }
  delete g_ndb;
  return 0;
}

// bug tests

static int
bugtest_4088()
{
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
    for (unsigned i = 0; i < pkcnt; i++) {
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
    for (unsigned i = 0; i < pkcnt; i++) {
      CHK(pktup[i].m_pk1 == tup.m_pk1);
      CHK(memcmp(pktup[i].m_pk2, tup.m_pk2, g_opt.m_pk2len) == 0);
    }
    CHK(memcmp(tup.m_blob1.m_val, tup.m_blob1.m_buf, 8 + g_opt.m_blob1.m_inline) == 0);
  }
  return 0;
}

static int
bugtest_2222()
{
  return 0;
}

static int
bugtest_3333()
{
  return 0;
}

static struct {
  int m_bug;
  int (*m_test)();
} g_bugtest[] = {
  { 4088, bugtest_4088 },
  { 2222, bugtest_2222 },
  { 3333, bugtest_3333 }
};

NDB_COMMAND(testOdbcDriver, "testBlobs", "testBlobs", "testBlobs", 65535)
{
  while (++argv, --argc > 0) {
    const char* arg = argv[0];
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
      putenv("NDB_BLOB_DEBUG=1");
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
    if (strcmp(arg, "-seed") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_seed = atoi(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-skip") == 0) {
      if (++argv, --argc > 0) {
        for (const char* p = argv[0]; *p != 0; p++) {
          skip(*p) = true;
        }
	continue;
      }
    }
    // metadata
    if (strcmp(arg, "-pk2len") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_pk2len = atoi(argv[0]);
        if (g_opt.m_pk2len == 0) {
          skip('i') = true;
          skip('r') = true;
        }
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
  if (testmain() == -1) {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  return NDBT_ProgramExit(NDBT_OK);
}

// vim: set sw=2 et:
