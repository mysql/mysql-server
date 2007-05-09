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
#include <OutputStream.hpp>
#include <NdbTest.hpp>
#include <NdbTick.h>
#include <my_sys.h>

struct Chr {
  NdbDictionary::Column::Type m_type;
  bool m_fixed;
  bool m_binary;
  uint m_len; // native
  uint m_bytelen; // in bytes
  uint m_totlen; // plus length bytes
  const char* m_cs;
  CHARSET_INFO* m_csinfo;
  uint m_mblen;
  bool m_caseins; // for latin letters
  Chr() :
    m_type(NdbDictionary::Column::Varchar),
    m_fixed(false),
    m_binary(false),
    m_len(55),
    m_bytelen(0),
    m_totlen(0),
    m_cs("latin1"),
    m_csinfo(0),
    m_caseins(true)
  {}
};

struct Opt {
  unsigned m_batch;
  bool m_core;
  bool m_dbg;
  const char* m_debug;
  bool m_fac;
  bool m_full;
  unsigned m_loop;
  bool m_min;
  unsigned m_parts;
  unsigned m_rows;
  int m_seed;
  const char* m_skip;
  const char* m_test;
  int m_blob_version;
  // metadata
  const char* m_tname;
  const char* m_x1name;  // hash index
  const char* m_x2name;  // ordered index
  unsigned m_pk1off;
  Chr m_pk2chr;
  bool m_pk2part;
  bool m_oneblob;
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
    m_debug(0),
    m_fac(false),
    m_full(false),
    m_loop(1),
    m_min(false),
    m_parts(10),
    m_rows(100),
    m_seed(-1),
    m_skip(0),
    m_test(0),
    m_blob_version(2),
    // metadata
    m_tname("TB1"),
    m_x1name("TB1X1"),
    m_x2name("TB1X2"),
    m_pk1off(0x12340000),
    m_pk2chr(),
    m_pk2part(false),
    m_oneblob(false),
    // perf
    m_tnameperf("TB2"),
    m_rowsperf(10000),
    // bugs
    m_bug(0),
    m_bugtest(0)
  {}
};

static void
printusage()
{
  Opt d;
  ndbout
    << "usage: testBlobs options [default/max]" << endl
    << "  -batch N    number of pk ops in batch [" << d.m_batch << "]" << endl
    << "  -core       dump core on error" << endl
    << "  -dbg        print program debug" << endl
    << "  -debug opt  also ndb api DBUG (if no ':' becomes d:t:F:L:o,opt)" << endl
    << "  -fac        fetch across commit in scan delete" << endl
    << "  -full       read/write only full blob values" << endl
    << "  -loop N     loop N times 0=forever [" << d.m_loop << "]" << endl
    << "  -min        small blob sizes" << endl
    << "  -parts N    max parts in blob value [" << d.m_parts << "]" << endl
    << "  -rows N     number of rows [" << d.m_rows << "]" << endl
    << "  -rowsperf N rows for performace test [" << d.m_rowsperf << "]" << endl
    << "  -seed N     random seed 0=loop number -1=random [" << d.m_seed << "]" << endl
    << "  -skip xxx   skip given tests (see list) [no tests]" << endl
    << "  -test xxx   only given tests (see list) [all tests]" << endl
    << "  -version N  blob version 1 or 2 [" << d.m_blob_version << "]" << endl
    << "metadata" << endl
    << "  -pk2len N   native length of PK2, zero omits PK2,PK3 [" << d.m_pk2chr.m_len << "]" << endl
    << "  -pk2fixed   PK2 is Char [default Varchar]" << endl
    << "  -pk2binary  PK2 is Binary or Varbinary" << endl
    << "  -pk2cs      PK2 charset or collation [" << d.m_pk2chr.m_cs << "]" << endl
    << "  -pk2part    partition primary table by PK2" << endl
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
    << "  d           delete, can skip only for one subtest" << endl
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
static const NdbRecord *g_key_record= 0;
static const NdbRecord *g_blob_record= 0;
static const NdbRecord *g_full_record= 0;
static const NdbRecord *g_idx_record= 0;
static const NdbRecord *g_ord_record= 0;
static unsigned g_pk1_offset= 0;
static unsigned g_pk2_offset= 0;
static unsigned g_pk3_offset= 0;
static unsigned g_blob1_offset= 0;
static unsigned g_blob2_offset= 0;
static unsigned g_rowsize= 0;

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


struct Bcol {
  int m_type;
  int m_version;
  bool m_nullable;
  uint m_inline;
  uint m_partsize;
  uint m_stripe;
  char m_btname[200];
  Bcol() { memset(this, 0, sizeof(*this)); }
};

static Bcol g_blob1;
static Bcol g_blob2;

static void
initblobs()
{
  {
    Bcol& b = g_blob1;
    b.m_type = NdbDictionary::Column::Text;
    b.m_version = g_opt.m_blob_version;
    b.m_nullable = false;
    b.m_inline = g_opt.m_min ? 8 : 240;
    b.m_partsize = g_opt.m_min ? 8 : 2000;
    b.m_stripe = b.m_version == 1 ? 4 : 0;
  }
  {
    Bcol& b = g_blob2;
    b.m_type = NdbDictionary::Column::Blob;
    b.m_version = g_opt.m_blob_version;
    b.m_nullable = true;
    b.m_inline = g_opt.m_min ? 9 : 99;
    b.m_partsize = g_opt.m_min ? 5 : 55;
    b.m_stripe = 3;
  }
}

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
  tab.setFragmentType(NdbDictionary::Object::FragAllLarge);
  const Chr& pk2chr = g_opt.m_pk2chr;
  // col PK1 - Uint32
  { NdbDictionary::Column col("PK1");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  // col BL1 - Text not-nullable
  { NdbDictionary::Column col("BL1");
    const Bcol& b = g_blob1;
    col.setType((NdbDictionary::Column::Type)b.m_type);
    col.setBlobVersion(b.m_version);
    col.setNullable(b.m_nullable);
    col.setInlineSize(b.m_inline);
    col.setPartSize(b.m_partsize);
    col.setStripeSize(b.m_stripe);
    tab.addColumn(col);
  }
  // col PK2 - Char or Varchar
  if (pk2chr.m_len != 0)
  { NdbDictionary::Column col("PK2");
    col.setType(pk2chr.m_type);
    col.setPrimaryKey(true);
    col.setLength(pk2chr.m_bytelen);
    if (pk2chr.m_csinfo != 0)
      col.setCharset(pk2chr.m_csinfo);
    if (g_opt.m_pk2part)
      col.setPartitionKey(true);
    tab.addColumn(col);
  }
  // col BL2 - Blob nullable
  if (! g_opt.m_oneblob)
  { NdbDictionary::Column col("BL2");
    const Bcol& b = g_blob2;
    col.setType((NdbDictionary::Column::Type)b.m_type);
    col.setBlobVersion(b.m_version);
    col.setNullable(b.m_nullable);
    col.setInlineSize(b.m_inline);
    col.setPartSize(b.m_partsize);
    col.setStripeSize(b.m_stripe);
    tab.addColumn(col);
  }
  // col PK3 - puts the Var* key PK2 between PK1 and PK3
  if (pk2chr.m_len != 0)
  { NdbDictionary::Column col("PK3");
    col.setType(NdbDictionary::Column::Smallunsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  // create table
  CHK(g_dic->createTable(tab) == 0);
  // unique hash index on PK2,PK3
  if (g_opt.m_pk2chr.m_len != 0)
  { NdbDictionary::Index idx(g_opt.m_x1name);
    idx.setType(NdbDictionary::Index::UniqueHashIndex);
    idx.setLogging(false);
    idx.setTable(g_opt.m_tname);
    idx.addColumnName("PK2");
    idx.addColumnName("PK3");
    CHK(g_dic->createIndex(idx) == 0);
  }
  // ordered index on PK2
  if (g_opt.m_pk2chr.m_len != 0)
  { NdbDictionary::Index idx(g_opt.m_x2name);
    idx.setType(NdbDictionary::Index::OrderedIndex);
    idx.setLogging(false);
    idx.setTable(g_opt.m_tname);
    idx.addColumnName("PK2");
    CHK(g_dic->createIndex(idx) == 0);
  }

  NdbDictionary::RecordSpecification spec[5];
  unsigned numpks= g_opt.m_pk2chr.m_len == 0 ? 1 : 3;
  unsigned numblobs= g_opt.m_oneblob ? 1 : 2;
  g_pk1_offset= 0;
  g_pk2_offset= g_pk1_offset + 4;
  g_pk3_offset= g_pk2_offset + g_opt.m_pk2chr.m_totlen;
  g_blob1_offset= g_pk3_offset + 2;
  g_blob2_offset= g_blob1_offset + sizeof(NdbBlob *);
  g_rowsize= g_blob2_offset + sizeof(NdbBlob *);

  const NdbDictionary::Table *dict_table;
  CHK((dict_table= g_dic->getTable(g_opt.m_tname)) != 0);
  memset(spec, 0, sizeof(spec));
  spec[0].column= dict_table->getColumn("PK1");
  spec[0].offset= g_pk1_offset;
  spec[numpks].column= dict_table->getColumn("BL1");
  spec[numpks].offset= g_blob1_offset;
  if (g_opt.m_pk2chr.m_len != 0)
  {
    spec[1].column= dict_table->getColumn("PK2");
    spec[1].offset= g_pk2_offset;
    spec[2].column= dict_table->getColumn("PK3");
    spec[2].offset= g_pk3_offset;
  }
  if (! g_opt.m_oneblob)
  {
    spec[numpks+1].column= dict_table->getColumn("BL2");
    spec[numpks+1].offset= g_blob2_offset;
  }
  CHK((g_key_record= g_dic->createRecord(dict_table, &spec[0], numpks,
                                         sizeof(spec[0]))) != 0);
  CHK((g_blob_record= g_dic->createRecord(dict_table, &spec[numpks], numblobs,
                                         sizeof(spec[0]))) != 0);
  CHK((g_full_record= g_dic->createRecord(dict_table, &spec[0], numpks+numblobs,
                                         sizeof(spec[0]))) != 0);

  if (g_opt.m_pk2chr.m_len != 0)
  {
    const NdbDictionary::Index *dict_index;
    CHK((dict_index= g_dic->getIndex(g_opt.m_x1name, g_opt.m_tname)) != 0);
    CHK((g_idx_record= g_dic->createRecord(dict_index, dict_table, &spec[1], 2,
                                           sizeof(spec[0]))) != 0);
    CHK((dict_index= g_dic->getIndex(g_opt.m_x2name, g_opt.m_tname)) != 0);
    CHK((g_ord_record= g_dic->createRecord(dict_index, dict_table, &spec[1], 1,
                                           sizeof(spec[0]))) != 0);
  }

  return 0;
}

// tuples

static unsigned
urandom(unsigned n)
{
  return n == 0 ? 0 : random() % n;
}

struct Bval {
  const Bcol& m_bcol;
  char* m_val;
  unsigned m_len;
  char* m_buf; // read/write buffer
  unsigned m_buflen;
  int m_error_code; // for testing expected error code
  Bval(const Bcol& bcol) :
    m_bcol(bcol),
    m_val(0),
    m_len(0),
    m_buf(0),
    m_buflen(0),
    m_error_code(0)
    {}
  ~Bval() { delete [] m_val; delete [] m_buf; }
  void alloc() {
    alloc(m_bcol.m_inline + m_bcol.m_partsize * g_opt.m_parts);
  }
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

NdbOut&
operator<<(NdbOut& out, const Bval& v)
{
  if (g_opt.m_min && v.m_val != 0) {
    out << "[" << v.m_len << "]";
    for (uint i = 0; i < v.m_len; i++) {
      const Bcol& b = v.m_bcol;
      if (i == b.m_inline ||
          (i > b.m_inline && (i - b.m_inline) % b.m_partsize == 0))
        out.print("|");
      out.print("%c", v.m_val[i]);
    }
  }
  return out;
}

struct Tup {
  bool m_exists;        // exists in table
  Uint32 m_pk1;         // in V1 primary keys concatenated like keyinfo
  char* m_pk2;
  char* m_pk2eq;        // equivalent (if case independent)
  Uint16 m_pk3;
  Bval m_bval1;
  Bval m_bval2;
  char *m_key_row;
  char *m_row;
  Uint32 m_frag;
  Tup() :
    m_exists(false),
    m_pk2(new char [g_opt.m_pk2chr.m_totlen + 1]), // nullterm for convenience
    m_pk2eq(new char [g_opt.m_pk2chr.m_totlen + 1]),
    m_bval1(g_blob1),
    m_bval2(g_blob2),
    m_key_row(new char[g_rowsize]),
    m_row(new char[g_rowsize]),
    m_frag(~(Uint32)0)
    {}
  ~Tup() {
    delete [] m_pk2;
    m_pk2 = 0;
    delete [] m_pk2eq;
    m_pk2eq = 0;
    delete [] m_key_row;
    m_key_row= 0;
    delete [] m_row;
    m_row= 0;
  }
  // alloc buffers of max size
  void alloc() {
    m_bval1.alloc();
    m_bval2.alloc();
  }
  void copyfrom(const Tup& tup) {
    assert(m_pk1 == tup.m_pk1);
    m_bval1.copyfrom(tup.m_bval1);
    m_bval2.copyfrom(tup.m_bval2);
  }
  /*
   * in V2 return pk2 or pk2eq at random
   * in V1 mixed cases do not work in general due to key packing
   * luckily they do work via mysql
   */
  char* pk2() {
    if (g_opt.m_blob_version == 1)
      return m_pk2;
    return urandom(2) == 0 ? m_pk2 : m_pk2eq;
  }
private:
  Tup(const Tup&);
  Tup& operator=(const Tup&);
};

static Tup* g_tups;

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
      v.m_val[i] = 'a' + urandom(26);
    v.m_val[v.m_len] = 0;
    v.m_buf = new char [v.m_len];
  }
  v.m_buflen = v.m_len;
  v.trash();
}

static void
calcBval(Tup& tup, bool keepsize)
{
  calcBval(g_blob1, tup.m_bval1, keepsize);
  if (! g_opt.m_oneblob)
    calcBval(g_blob2, tup.m_bval2, keepsize);
}

// dont remember what the keepsize was for..
static void
calcTups(bool keys, bool keepsize = false)
{
  for (uint k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    if (keys) {
      tup.m_pk1 = g_opt.m_pk1off + k;
      {
        const Chr& c = g_opt.m_pk2chr;
        char* const p = tup.m_pk2;
        char* const q = tup.m_pk2eq;
        uint len = urandom(c.m_len + 1);
        uint i = 0;
        if (! c.m_fixed) {
          *(uchar*)&p[0] = *(uchar*)&q[0] = len;
          i++;
        }
        uint j = 0;
        while (j < len) {
          // mixed case for distribution check
          if (urandom(3) == 0) {
            uint u = urandom(26);
            p[i] = 'A' + u;
            q[i] = c.m_caseins ? 'a' + u : 'A' + u;
          } else {
            uint u = urandom(26);
            p[i] = 'a' + u;
            q[i] = c.m_caseins ? 'A' + u : 'a' + u;
          }
          i++;
          j++;
        }
        while (j < c.m_bytelen) {
          if (c.m_fixed)
            p[i] = q[i] = 0x20;
          else
            p[i] = q[i] = '#'; // garbage
          i++;
          j++;
        }
        assert(i == c.m_totlen);
        p[i] = q[i] = 0; // convenience
      }
      tup.m_pk3 = (Uint16)k;
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
  DBG("setValue " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null << " " << v);
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
  CHK(setBlobValue(g_bh1, tup.m_bval1, error_code) == 0);
  if (! g_opt.m_oneblob)
    CHK(setBlobValue(g_bh2, tup.m_bval2, error_code) == 0);
  return 0;
}

static int
getBlobValue(NdbBlob* h, const Bval& v)
{
  DBG("getValue " <<  h->getColumn()->getName() << " buflen=" << v.m_buflen);
  CHK(h->getValue(v.m_buf, v.m_buflen) == 0);
  return 0;
}

static int
getBlobValue(const Tup& tup)
{
  CHK(getBlobValue(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob)
    CHK(getBlobValue(g_bh2, tup.m_bval2) == 0);
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
  CHK(verifyBlobValue(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob)
    CHK(verifyBlobValue(g_bh2, tup.m_bval2) == 0);
  return 0;
}

// readData / writeData

static int
writeBlobData(NdbBlob* h, const Bval& v)
{
  bool null = (v.m_val == 0);
  bool isNull;
  unsigned len;
  DBG("write " <<  h->getColumn()->getName() << " len=" << v.m_len << " null=" << null << " " << v);
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
  tup.m_bval1.m_error_code = error_code;
  CHK(writeBlobData(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob) {
    tup.m_bval2.m_error_code = error_code;
    CHK(writeBlobData(g_bh2, tup.m_bval2) == 0);
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
  CHK(readBlobData(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob)
    CHK(readBlobData(g_bh2, tup.m_bval2) == 0);
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
  CHK(setBlobWriteHook(g_bh1, tup.m_bval1, error_code) == 0);
  if (! g_opt.m_oneblob)
    CHK(setBlobWriteHook(g_bh2, tup.m_bval2, error_code) == 0);
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
  CHK(setBlobReadHook(g_bh1, tup.m_bval1) == 0);
  if (! g_opt.m_oneblob)
    CHK(setBlobReadHook(g_bh2, tup.m_bval2) == 0);
  return 0;
}

// verify blob data

static int
verifyHeadInline(const Bcol& b, const Bval& v, NdbRecAttr* ra)
{
  if (v.m_val == 0) {
    CHK(ra->isNULL() == 1);
  } else {
    CHK(ra->isNULL() == 0);
    NdbBlob::Head head;
    NdbBlob::unpackBlobHead(head, ra->aRef(), b.m_version);
    CHK(head.length == v.m_len);
    const char* data = ra->aRef() + head.headsize;
    for (unsigned i = 0; i < head.length && i < b.m_inline; i++)
      CHK(data[i] == v.m_val[i]);
  }
  return 0;
}

static int
verifyHeadInline(Tup& tup)
{
  DBG("verifyHeadInline pk1=" << hex << tup.m_pk1);
  CHK((g_con = g_ndb->startTransaction()) != 0);
  CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
  CHK(g_opr->readTuple() == 0);
  CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
  if (g_opt.m_pk2chr.m_len != 0) {
    CHK(g_opr->equal("PK2", tup.pk2()) == 0);
    CHK(g_opr->equal("PK3", (char*)&tup.m_pk3) == 0);
  }
  NdbRecAttr* ra1;
  NdbRecAttr* ra2;
  NdbRecAttr* ra_frag;
  CHK((ra1 = g_opr->getValue("BL1")) != 0);
  if (! g_opt.m_oneblob)
    CHK((ra2 = g_opr->getValue("BL2")) != 0);
  CHK((ra_frag = g_opr->getValue(NdbDictionary::Column::FRAGMENT)) != 0);
  if (tup.m_exists) {
    CHK(g_con->execute(Commit, AbortOnError) == 0);
    tup.m_frag = ra_frag->u_32_value();
    DBG("fragment id: " << tup.m_frag);
    DBG("verifyHeadInline BL1");
    CHK(verifyHeadInline(g_blob1, tup.m_bval1, ra1) == 0);
    if (! g_opt.m_oneblob) {
      DBG("verifyHeadInline BL2");
      CHK(verifyHeadInline(g_blob2, tup.m_bval2, ra2) == 0);
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

static unsigned
getvarsize(const char* buf)
{
  const unsigned char* p = (const unsigned char*)buf;
  return p[0] + (p[1] << 8);
}

static int
verifyBlobTable(const Bval& v, Uint32 pk1, Uint32 frag, bool exists)
{
  const Bcol& b = v.m_bcol;
  DBG("verify " << b.m_btname << " pk1=" << hex << pk1);
  NdbRecAttr* ra_pk = 0; // V1
  NdbRecAttr* ra_pk1 = 0; // V2
  NdbRecAttr* ra_pk2 = 0; // V2
  NdbRecAttr* ra_pk3 = 0; // V2
  NdbRecAttr* ra_part = 0;
  NdbRecAttr* ra_data = 0;
  NdbRecAttr* ra_frag = 0;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  CHK((g_ops = g_con->getNdbScanOperation(b.m_btname)) != 0);
  CHK(g_ops->readTuples() == 0);
  if (b.m_version == 1) {
    CHK((ra_pk = g_ops->getValue("PK")) != 0);
    CHK((ra_part = g_ops->getValue("PART")) != 0);
    CHK((ra_data = g_ops->getValue("DATA")) != 0);
  } else {
    CHK((ra_pk1 = g_ops->getValue("PK1")) != 0);
    if (g_opt.m_pk2chr.m_len != 0) {
      CHK((ra_pk2 = g_ops->getValue("PK2")) != 0);
      CHK((ra_pk3 = g_ops->getValue("PK3")) != 0);
    }
    CHK((ra_part = g_ops->getValue("NDB$PART")) != 0);
    CHK((ra_data = g_ops->getValue("NDB$DATA")) != 0);
  }
  CHK((ra_frag = g_ops->getValue(NdbDictionary::Column::FRAGMENT)) != 0);
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
    if (b.m_version == 1) {
      if (pk1 != ra_pk->u_32_value())
        continue;
    } else {
      if (pk1 != ra_pk1->u_32_value())
        continue;
    }
    Uint32 part = ra_part->u_32_value();
    DBG("part " << part << " of " << partcount);
    CHK(part < partcount && ! seen[part]);
    seen[part] = 1;
    unsigned n = b.m_inline + part * b.m_partsize;
    assert(exists && v.m_val != 0 && n < v.m_len);
    unsigned m = v.m_len - n;
    if (m > b.m_partsize)
      m = b.m_partsize;
    const char* data = ra_data->aRef();
    if (b.m_version == 1)
      ;
    else {
      unsigned sz = getvarsize(data);
      DBG("varsize " << sz);
      CHK(sz <= b.m_partsize);
      data += 2;
      if (part + 1 < partcount)
        CHK(sz == b.m_partsize);
      else
        CHK(sz == m);
    }
    CHK(memcmp(data, v.m_val + n, m) == 0);
    if (b.m_version == 1) {
      char fillchr;
      if (b.m_type == NdbDictionary::Column::Text)
        fillchr = 0x20;
      else
        fillchr = 0x0;
      uint i = m;
      while (i < b.m_partsize) {
        CHK(data[i] == fillchr);
        i++;
      }
    }
    Uint32 frag2 = ra_frag->u_32_value();
    DBG("frags main=" << frag << " blob=" << frag2 << " stripe=" << b.m_stripe);
    if (b.m_stripe == 0)
      CHK(frag == frag2);
  }
  for (unsigned i = 0; i < partcount; i++)
    CHK(seen[i] == 1);
  delete [] seen;
  g_ndb->closeTransaction(g_con);
  g_ops = 0;
  g_con = 0;
  return 0;
}

static int
verifyBlobTable(const Tup& tup)
{
  CHK(verifyBlobTable(tup.m_bval1, tup.m_pk1, tup.m_frag, tup.m_exists) == 0);
  if (! g_opt.m_oneblob)
    CHK(verifyBlobTable(tup.m_bval2, tup.m_pk1, tup.m_frag, tup.m_exists) == 0);
  return 0;
}

static int
verifyBlob()
{
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
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
    memcpy(&tup.m_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
    if (g_opt.m_pk2chr.m_len != 0) {
      memcpy(&tup.m_row[g_pk2_offset], tup.m_pk2, g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    }
    CHK((g_opr = g_con->insertTuple(g_full_record, tup.m_row)) != 0);
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
    memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
    if (g_opt.m_pk2chr.m_len != 0) {
      memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    }
    if (urandom(2) == 0)
      CHK((g_opr = g_con->readTuple(g_key_record, tup.m_key_row,
                                    g_blob_record, tup.m_row)) != 0);
    else
      CHK((g_opr = g_con->readTuple(g_key_record, tup.m_key_row,
                                    g_blob_record, tup.m_row,
                                    NdbOperation::LM_CommittedRead)) != 0);
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
      memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
      if (g_opt.m_pk2chr.m_len != 0) {
        memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
        memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
      }
      if (mode == 0) {
        DBG("using updateTuple");
        CHK((g_opr= g_con->updateTuple(g_key_record, tup.m_key_row,
                                       g_blob_record, tup.m_row)) != 0);
      } else if (mode == 1) {
        DBG("using readTuple exclusive");
        CHK((g_opr= g_con->readTuple(g_key_record, tup.m_key_row,
                                     g_blob_record, tup.m_row,
                                     NdbOperation::LM_Exclusive)) != 0);
      } else {
        DBG("using readTuple - will fail and retry");
        CHK((g_opr= g_con->readTuple(g_key_record, tup.m_key_row,
                                     g_blob_record, tup.m_row)) != 0);
      }
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
    memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
    memcpy(&tup.m_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
    if (g_opt.m_pk2chr.m_len != 0) {
      memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
      memcpy(&tup.m_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    }
    CHK((g_opr= g_con->writeTuple(g_key_record, tup.m_key_row,
                                  g_full_record, tup.m_row)) != 0);
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
    memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
    if (g_opt.m_pk2chr.m_len != 0) {
      memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    }
    CHK((g_opr= g_con->deleteTuple(g_key_record, tup.m_key_row)) != 0);
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
  no_tup.m_pk1 = 0xb1ff;
  const Chr& pk2chr = g_opt.m_pk2chr;
  if (pk2chr.m_len != 0) {
    char* const p = no_tup.m_pk2;
    uint len = urandom(pk2chr.m_len + 1);
    uint i = 0;
    if (! pk2chr.m_fixed) {
      *(uchar*)&p[0] = len;
      i++;
    }
    uint j = 0;
    while (j < len) {
      p[i] = "b1ff"[j % 4];
      i++;
      j++;
    }
  }
  no_tup.m_pk3 = 0xb1ff;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  Tup& tup =  no_tup;
  DBG("deletePk pk1=" << hex << tup.m_pk1);
  CHK((g_opr = g_con->getNdbOperation(g_opt.m_tname)) != 0);
  CHK(g_opr->deleteTuple() == 0);
  CHK(g_opr->equal("PK1", tup.m_pk1) == 0);
  if (pk2chr.m_len != 0) {
    CHK(g_opr->equal("PK2", tup.m_pk2) == 0);
    CHK(g_opr->equal("PK3", (char*)&tup.m_pk2) == 0);
  }
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
    memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
    memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    if (urandom(2) == 0)
      CHK((g_opr= g_con->readTuple(g_idx_record, tup.m_key_row,
                                   g_blob_record, tup.m_row)) != 0);
    else
      CHK((g_opr= g_con->readTuple(g_idx_record, tup.m_key_row,
                                   g_blob_record, tup.m_row,
                                   NdbOperation::LM_CommittedRead)) != 0);
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
    // verify lock mode upgrade (already done by NdbIndexOperation)
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
updateIdx(int style)
{
  DBG("--- updateIdx " << stylename[style] << " ---");
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("updateIdx pk1=" << hex << tup.m_pk1);
    // skip 4275 testing
    CHK((g_con = g_ndb->startTransaction()) != 0);
    memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
    memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    CHK((g_opr= g_con->updateTuple(g_idx_record, tup.m_key_row,
                                   g_blob_record, tup.m_row)) != 0);
    CHK(getBlobHandles(g_opr) == 0);
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
    g_opr = 0;
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
    memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
    memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    memcpy(&tup.m_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
    memcpy(&tup.m_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
    memcpy(&tup.m_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    CHK((g_opr= g_con->writeTuple(g_idx_record, tup.m_key_row,
                                  g_full_record, tup.m_row)) != 0);
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
deleteIdx()
{
  DBG("--- deleteIdx ---");
  unsigned n = 0;
  CHK((g_con = g_ndb->startTransaction()) != 0);
  for (unsigned k = 0; k < g_opt.m_rows; k++) {
    Tup& tup = g_tups[k];
    DBG("deleteIdx pk1=" << hex << tup.m_pk1);
    memcpy(&tup.m_key_row[g_pk2_offset], tup.pk2(), g_opt.m_pk2chr.m_totlen);
    memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    CHK((g_opr= g_con->deleteTuple(g_idx_record, tup.m_key_row)) != 0);
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
  if (urandom(2) == 0)
    if (! idx)
      CHK((g_ops= g_con->scanTable(g_full_record,
                                   NdbOperation::LM_Read)) != 0);
  else 
      CHK((g_ops= g_con->scanIndex(g_ord_record, NULL, NULL, 0, g_full_record,
                                   NdbOperation::LM_Read)) != 0);
  else
    if (! idx)
      CHK((g_ops= g_con->scanTable(g_full_record,
                                   NdbOperation::LM_CommittedRead)) != 0);
    else
      CHK((g_ops= g_con->scanIndex(g_ord_record, NULL, NULL, 0, g_full_record,
                                   NdbOperation::LM_CommittedRead)) != 0);
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
    const char *out_row= NULL;
    int ret;

    CHK((ret = g_ops->nextResult(out_row, true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    memcpy(&tup.m_pk1, &out_row[g_pk1_offset], sizeof(tup.m_pk1));
    if (g_opt.m_pk2chr.m_len != 0)
    {
      memcpy(tup.m_pk2, &out_row[g_pk2_offset], g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_pk3, &out_row[g_pk3_offset], sizeof(tup.m_pk3));
    }

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
  if (! idx)
    CHK((g_ops= g_con->scanTable(g_key_record,
                                 NdbOperation::LM_Exclusive)) != 0);
  else
    CHK((g_ops= g_con->scanIndex(g_ord_record, NULL, NULL, 0, g_key_record,
                                 NdbOperation::LM_Exclusive)) != 0);
  CHK(g_con->execute(NoCommit) == 0);
  unsigned rows = 0;
  while (1) {
    const char *out_row= NULL;
    int ret;

    CHK((ret = g_ops->nextResult(out_row, true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    memcpy(&tup.m_pk1, &out_row[g_pk1_offset], sizeof(tup.m_pk1));
    if (g_opt.m_pk2chr.m_len != 0) {
      memcpy(tup.m_pk2, &out_row[g_pk2_offset], g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_pk3, &out_row[g_pk3_offset], sizeof(tup.m_pk3));
    }

    DBG("updateScan" << (idx ? "Idx" : "") << " pk1=" << hex << tup.m_pk1);
    Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
    CHK(k < g_opt.m_rows && g_tups[k].m_exists);
    // calculate new blob values
    calcBval(g_tups[k], false);
    tup.copyfrom(g_tups[k]);
    // cannot do 4275 testing, scan op error code controls execution
    CHK((g_opr = g_ops->updateCurrentTuple(g_con, g_blob_record, tup.m_row)) != 0);
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
  if (! idx)
    CHK((g_ops= g_con->scanTable(g_key_record,
                                 NdbOperation::LM_Exclusive)) != 0);
  else
    CHK((g_ops= g_con->scanIndex(g_ord_record, NULL, NULL, 0, g_key_record,
                                 NdbOperation::LM_Exclusive)) != 0);
  CHK(g_con->execute(NoCommit) == 0);
  unsigned rows = 0;
  unsigned n = 0;
  while (1) {
    const char *out_row= NULL;
    int ret;

    CHK((ret = g_ops->nextResult(out_row, true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    memcpy(&tup.m_pk1, &out_row[g_pk1_offset], sizeof(tup.m_pk1));
    if (g_opt.m_pk2chr.m_len != 0)
    {
      memcpy(tup.m_pk2, &out_row[g_pk2_offset], g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_pk3, &out_row[g_pk3_offset], sizeof(tup.m_pk3));
    }

    while (1) {
      DBG("deleteScan" << (idx ? "Idx" : "") << " pk1=" << hex << tup.m_pk1);
      Uint32 k = tup.m_pk1 - g_opt.m_pk1off;
      CHK(k < g_opt.m_rows && g_tups[k].m_exists);
      g_tups[k].m_exists = false;
      CHK(g_ops->deleteCurrentTuple(g_con, g_key_record) != NULL);
      rows++;
      tup.m_pk1 = (Uint32)-1;
      memset(tup.m_pk2, 'x', g_opt.m_pk2chr.m_len);
      CHK((ret = g_ops->nextResult(out_row, false)) == 0 || ret == 1 || ret == 2);
      if (ret == 0)
      {
        memcpy(&tup.m_pk1, &out_row[g_pk1_offset], sizeof(tup.m_pk1));
        if (g_opt.m_pk2chr.m_len != 0)
        {
          memcpy(tup.m_pk2, &out_row[g_pk2_offset], g_opt.m_pk2chr.m_totlen);
          memcpy(&tup.m_pk3, &out_row[g_pk3_offset], sizeof(tup.m_pk3));
        }
      }
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
  initblobs();
  CHK(dropTable() == 0);
  CHK(createTable() == 0);
  g_tups = new Tup [g_opt.m_rows];
  Bcol& b1 = g_blob1;
  CHK(NdbBlob::getBlobTableName(b1.m_btname, g_ndb, g_opt.m_tname, "BL1") == 0);
  DBG("BL1: inline=" << b1.m_inline << " part=" << b1.m_partsize << " table=" << b1.m_btname);
  if (! g_opt.m_oneblob) {
    Bcol& b2 = g_blob2;
    CHK(NdbBlob::getBlobTableName(b2.m_btname, g_ndb, g_opt.m_tname, "BL2") == 0);
    DBG("BL2: inline=" << b2.m_inline << " part=" << b2.m_partsize << " table=" << b2.m_btname);
  }
  if (g_opt.m_seed == -1)
    g_opt.m_seed = getpid();
  if (g_opt.m_seed != 0) {
    DBG("random seed = " << g_opt.m_seed);
    srandom(g_opt.m_seed);
  }
  for (g_loop = 0; g_opt.m_loop == 0 || g_loop < g_opt.m_loop; g_loop++) {
    int style;
    DBG("=== loop " << g_loop << " ===");
    if (g_opt.m_seed == 0)
      srandom(g_loop);
    if (g_opt.m_bugtest != 0) {
      // test some bug# instead
      CHK((*g_opt.m_bugtest)() == 0);
      continue;
    }
    // pk
    for (style = 0; style <= 2; style++) {
      if (! testcase('k') || ! testcase(style))
        continue;
      DBG("--- pk ops " << stylename[style] << " ---");
      if (testcase('n')) {
        calcTups(true);
        CHK(insertPk(style) == 0);
        CHK(verifyBlob() == 0);
        CHK(readPk(style) == 0);
        if (testcase('u')) {
          calcTups(false);
          CHK(updatePk(style) == 0);
          CHK(verifyBlob() == 0);
          CHK(readPk(style) == 0);
        }
        if (testcase('d')) {
          CHK(deletePk() == 0);
          CHK(deleteNoPk() == 0);
          CHK(verifyBlob() == 0);
        }
      }
      if (testcase('w')) {
        calcTups(true);
        CHK(writePk(style) == 0);
        CHK(verifyBlob() == 0);
        CHK(readPk(style) == 0);
        if (testcase('u')) {
          calcTups(false);
          CHK(writePk(style) == 0);
          CHK(verifyBlob() == 0);
          CHK(readPk(style) == 0);
        }
        if (testcase('d')) {
          CHK(deletePk() == 0);
          CHK(deleteNoPk() == 0);
          CHK(verifyBlob() == 0);
        }
      }
    }
    // hash index
    for (style = 0; style <= 2; style++) {
      if (! testcase('i') || ! testcase(style))
        continue;
      DBG("--- idx ops " << stylename[style] << " ---");
      if (testcase('n')) {
        calcTups(true);
        CHK(insertPk(style) == 0);
        CHK(verifyBlob() == 0);
        CHK(readIdx(style) == 0);
        if (testcase('u')) {
          calcTups(false);
          CHK(updateIdx(style) == 0);
          CHK(verifyBlob() == 0);
          CHK(readIdx(style) == 0);
        }
        if (testcase('d')) {
          CHK(deleteIdx() == 0);
          CHK(verifyBlob() == 0);
        }
      }
      if (testcase('w')) {
        calcTups(false);
        CHK(writePk(style) == 0);
        CHK(verifyBlob() == 0);
        CHK(readIdx(style) == 0);
        if (testcase('u')) {
          calcTups(false);
          CHK(writeIdx(style) == 0);
          CHK(verifyBlob() == 0);
          CHK(readIdx(style) == 0);
        }
        if (testcase('d')) {
          CHK(deleteIdx() == 0);
          CHK(verifyBlob() == 0);
        }
      }
    }
    // scan table
    for (style = 0; style <= 2; style++) {
      if (! testcase('s') || ! testcase(style))
        continue;
      DBG("--- table scan " << stylename[style] << " ---");
      calcTups(true);
      CHK(insertPk(style) == 0);
      CHK(verifyBlob() == 0);
      CHK(readScan(style, false) == 0);
      if (testcase('u')) {
        CHK(updateScan(style, false) == 0);
        CHK(verifyBlob() == 0);
      }
      if (testcase('d')) {
        CHK(deleteScan(false) == 0);
        CHK(verifyBlob() == 0);
      }
    }
    // scan index
    for (style = 0; style <= 2; style++) {
      if (! testcase('r') || ! testcase(style))
        continue;
      DBG("--- index scan " << stylename[style] << " ---");
      calcTups(true);
      CHK(insertPk(style) == 0);
      CHK(verifyBlob() == 0);
      CHK(readScan(style, true) == 0);
      if (testcase('u')) {
        CHK(updateScan(style, true) == 0);
        CHK(verifyBlob() == 0);
      }
      if (testcase('d')) {
        CHK(deleteScan(true) == 0);
        CHK(verifyBlob() == 0);
      }
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
    col.setBlobVersion(g_opt.m_blob_version);
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
  const uint scan_loops = 10;
  {
    DBG("--- scan read char ---");
    Uint32 a;
    char b[20];
    uint i;
    for (i = 0; i < scan_loops; i++) {
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
      g_ndb->closeTransaction(g_con); g_ops = 0;
      g_con = 0;
    }
    DBG(t1.time());
  }
  // scan read text
  {
    DBG("--- read text ---");
    Uint32 a;
    char c[20];
    uint i;
    for (i = 0; i < scan_loops; i++) {
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
      g_bh1 = 0;
      g_ops = 0;
      g_ndb->closeTransaction(g_con); g_con = 0;
    }
    DBG(t2.time());
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
  calcTups(true);
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
    assert(tup.m_bval1.m_buf != 0);
    CHK(g_opx->getValue("BL1", (char*)tup.m_bval1.m_buf) != 0);
    // execute
    // BUG 4088: gets 1 tckeyconf, 1 tcindxconf, then hangs
    CHK(g_con->execute(Commit) == 0);
    // verify
    for (i = 0; i < pkcnt; i++) {
      CHK(pktup[i].m_pk1 == tup.m_pk1);
      CHK(memcmp(pktup[i].m_pk2, tup.m_pk2, g_opt.m_pk2chr.m_len) == 0);
    }
    CHK(memcmp(tup.m_bval1.m_val, tup.m_bval1.m_buf, 8 + g_blob1.m_inline) == 0);
  }
  return 0;
}

static int
bugtest_27018()
{
  DBG("bug test 27018 - middle partial part write clobbers rest of part");

  // insert rows
  calcTups(true);
  CHK(insertPk(false) == 0);
  // new trans
  for (unsigned k= 0; k < g_opt.m_rows; k++)
  {
    Tup& tup= g_tups[k];

    /* Update one byte in random position. */
    Uint32 offset= urandom(tup.m_bval1.m_len + 1);
    if (offset == tup.m_bval1.m_len) {
      // testing write at end is another problem..
      continue;
    }
    //DBG("len=" << tup.m_bval1.m_len << " offset=" << offset);

    CHK((g_con= g_ndb->startTransaction()) != 0);
    memcpy(&tup.m_key_row[g_pk1_offset], &tup.m_pk1, sizeof(tup.m_pk1));
    if (g_opt.m_pk2chr.m_len != 0) {
      memcpy(&tup.m_key_row[g_pk2_offset], tup.m_pk2, g_opt.m_pk2chr.m_totlen);
      memcpy(&tup.m_key_row[g_pk3_offset], &tup.m_pk3, sizeof(tup.m_pk3));
    }
    CHK((g_opr= g_con->updateTuple(g_key_record, tup.m_key_row,
                                   g_blob_record, tup.m_row)) != 0);
    CHK(getBlobHandles(g_opr) == 0);
    CHK(g_con->execute(NoCommit) == 0);

    tup.m_bval1.m_buf[0]= 0xff ^ tup.m_bval1.m_val[offset];
    CHK(g_bh1->setPos(offset) == 0);
    CHK(g_bh1->writeData(&(tup.m_bval1.m_buf[0]), 1) == 0);
    CHK(g_con->execute(Commit) == 0);
    g_ndb->closeTransaction(g_con);

    CHK((g_con= g_ndb->startTransaction()) != 0);
    CHK((g_opr= g_con->readTuple(g_key_record, tup.m_key_row,
                                 g_blob_record, tup.m_row)) != 0);
    CHK(getBlobHandles(g_opr) == 0);

    CHK(g_bh1->getValue(tup.m_bval1.m_buf, tup.m_bval1.m_len) == 0);
    CHK(g_con->execute(Commit) == 0);

    Uint64 len= ~0;
    CHK(g_bh1->getLength(len) == 0 && len == tup.m_bval1.m_len);
    tup.m_bval1.m_buf[offset]^= 0xff;
    //CHK(memcmp(tup.m_bval1.m_buf, tup.m_bval1.m_val, tup.m_bval1.m_len) == 0);
    Uint32 i = 0;
    while (i < tup.m_bval1.m_len) {
      CHK(tup.m_bval1.m_buf[i] == tup.m_bval1.m_val[i]);
      i++;
    }

    g_ndb->closeTransaction(g_con);
  }
  CHK(deletePk() == 0);

  return 0;
}


struct bug27370_data {
  Ndb *m_ndb;
  char m_current_write_value;
  char *m_writebuf;
  Uint32 m_blob1_size;
  char *m_key_row;
  char *m_read_row;
  char *m_write_row;
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
    memcpy(data->m_write_row, data->m_key_row, g_rowsize);
    if ((opr= con->writeTuple(g_key_record, data->m_key_row,
                              g_full_record, data->m_write_row)) == 0)
      return (void *)"Failed to create operation";
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

  CHK((data.m_key_row= new char[g_rowsize*3]) != 0);
  data.m_read_row= data.m_key_row + g_rowsize;
  data.m_write_row= data.m_read_row + g_rowsize;

  data.m_ndb= new Ndb(g_ncc, "TEST_DB");
  CHK(data.m_ndb->init(20) == 0);
  CHK(data.m_ndb->waitUntilReady() == 0);

  data.m_current_write_value= 0;
  data.m_blob1_size= g_blob1.m_inline + 10 * g_blob1.m_partsize;
  CHK((data.m_writebuf= new char [data.m_blob1_size]) != 0);
  Uint32 pk1_value= 27370;
  memcpy(&data.m_key_row[g_pk1_offset], &pk1_value, sizeof(pk1_value));
  if (g_opt.m_pk2chr.m_len != 0)
  {
    memset(&data.m_key_row[g_pk2_offset], 'x', g_opt.m_pk2chr.m_totlen);
    if (!g_opt.m_pk2chr.m_fixed)
      data.m_key_row[g_pk2_offset]= urandom(g_opt.m_pk2chr.m_len + 1);
    Uint16 pk3_value= 27370;
    memcpy(&data.m_key_row[g_pk3_offset], &pk3_value, sizeof(pk3_value));
  }
  data.m_thread_stop= false;

  memset(data.m_writebuf, data.m_current_write_value, data.m_blob1_size);
  data.m_current_write_value++;

  CHK((g_con= g_ndb->startTransaction()) != 0);
  memcpy(data.m_write_row, data.m_key_row, g_rowsize);
  CHK((g_opr= g_con->writeTuple(g_key_record, data.m_key_row,
                                g_full_record, data.m_write_row)) != 0);
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
    CHK((g_opr= g_con->readTuple(g_key_record, data.m_key_row,
                                 g_blob_record, data.m_read_row,
                                 NdbOperation::LM_CommittedRead)) != 0);
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
    CHK((g_ops= g_con->scanTable(g_full_record,
                                 NdbOperation::LM_CommittedRead)) != 0);
    CHK((g_bh1= g_ops->getBlobHandle("BL1")) != 0);
    CHK(g_con->execute(NoCommit, AbortOnError, 1) == 0);
    const char *out_row;
    CHK(g_ops->nextResult(out_row, true) == 0);

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

    CHK(g_ops->nextResult(out_row, true) == 1);
    g_ndb->closeTransaction(g_con);
    g_con= NULL;
  }

  data.m_thread_stop= true;
  void *thread_return;
  CHK(pthread_join(thread_handle, &thread_return) == 0);
  DBG("bug 27370 - thread return status: " <<
      (thread_return ? (char *)thread_return : "<null>"));
  CHK(thread_return == 0);

  delete [] data.m_key_row;
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
  // log the invocation
  char cmdline[512];
  {
    const char* progname =
      strchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
    strcpy(cmdline, progname);
    for (int i = 1; i < argc; i++) {
      strcat(cmdline, " ");
      strcat(cmdline, argv[i]);
    }
  }
  Chr& pk2chr = g_opt.m_pk2chr;
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
    if (strcmp(arg, "-debug") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_dbg = true;
        g_opt.m_debug = strdup(argv[0]);
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
    if (strcmp(arg, "-min") == 0) {
      g_opt.m_min = true;
      continue;
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
    if (strcmp(arg, "-version") == 0) {
      if (++argv, --argc > 0) {
	g_opt.m_blob_version = atoi(argv[0]);
        if (g_opt.m_blob_version == 1 || g_opt.m_blob_version == 2)
          continue;
      }
    }
    // metadata
    if (strcmp(arg, "-pk2len") == 0) {
      if (++argv, --argc > 0) {
	pk2chr.m_len = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-pk2fixed") == 0) {
      pk2chr.m_fixed = true;
      continue;
    }
    if (strcmp(arg, "-pk2binary") == 0) {
      pk2chr.m_binary = true;
      continue;
    }
    if (strcmp(arg, "-pk2cs") == 0) {
      if (++argv, --argc > 0) {
        pk2chr.m_cs = strdup(argv[0]);
	continue;
      }
    }
    if (strcmp(arg, "-pk2part") == 0) {
      g_opt.m_pk2part = true;
      continue;
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
    if (strcmp(arg, "-?") == 0 || strcmp(arg, "-h") == 0) {
      printusage();
      goto success;
    }
    ndbout << "unknown option " << arg << endl;
    goto wrongargs;
  }
  if (g_opt.m_debug != 0) {
    if (strchr(g_opt.m_debug, ':') == 0) {
      const char* s = "d:t:F:L:o,";
      char* t = new char [strlen(s) + strlen(g_opt.m_debug) + 1];
      strcpy(t, s);
      strcat(t, g_opt.m_debug);
      g_opt.m_debug = t;
    }
    DBUG_PUSH(g_opt.m_debug);
    ndbout.m_out = new FileOutputStream(DBUG_FILE);
  }
  if (pk2chr.m_len == 0) {
    char b[100];
    b[0] = 0;
    if (g_opt.m_skip != 0)
      strcpy(b, g_opt.m_skip);
    strcat(b, "i");
    strcat(b, "r");
    g_opt.m_skip = strdup(b);
  }
  if (pk2chr.m_len != 0) {
    Chr& c = pk2chr;
    if (c.m_binary) {
      if (c.m_fixed)
        c.m_type = NdbDictionary::Column::Binary;
      else
        c.m_type = NdbDictionary::Column::Varbinary;
      c.m_mblen = 1;
      c.m_cs = 0;
    } else {
      assert(c.m_cs != 0);
      if (c.m_fixed)
        c.m_type = NdbDictionary::Column::Char;
      else
        c.m_type = NdbDictionary::Column::Varchar;
      c.m_csinfo = get_charset_by_name(c.m_cs, MYF(0));
      if (c.m_csinfo == 0)
        c.m_csinfo = get_charset_by_csname(c.m_cs, MY_CS_PRIMARY, MYF(0));
      if (c.m_csinfo == 0) {
        ndbout << "unknown charset " << c.m_cs << endl;
        goto wrongargs;
      }
      c.m_mblen = c.m_csinfo->mbmaxlen;;
      if (c.m_mblen == 0)
        c.m_mblen = 1;
    }
    c.m_bytelen = c.m_len * c.m_mblen;
    if (c.m_bytelen > 255) {
      ndbout << "length of pk2 in bytes exceeds 255" << endl;
      goto wrongargs;
    }
    if (c.m_fixed)
      c.m_totlen = c.m_bytelen;
    else
      c.m_totlen = 1 + c.m_bytelen;
    c.m_caseins = false;
    if (c.m_cs != 0) {
      CHARSET_INFO* info = c.m_csinfo;
      const char* p = "ABCxyz";
      const char* q = "abcXYZ";
      int e;
      if ((*info->cset->well_formed_len)(info, p, p + 6, 999, &e) != 6) {
        ndbout << "charset does not contain ascii" << endl;
        goto wrongargs;
      }
      if ((*info->coll->strcasecmp)(info, p, q) == 0) {
        c.m_caseins = true;
      }
      ndbout << "charset: " << c.m_cs << " caseins: " << c.m_caseins << endl;
    }
  }
  ndbout << cmdline << endl;
  g_ncc = new Ndb_cluster_connection();
  if (g_ncc->connect(30) != 0 || testmain() == -1 || testperf() == -1) {
    ndbout << "line " << __LINE__ << " FAIL loop=" << g_loop << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  delete g_ncc;
  g_ncc = 0;
success:
  return NDBT_ProgramExit(NDBT_OK);
wrongargs:
  return NDBT_ProgramExit(NDBT_WRONGARGS);
}

// vim: set sw=2 et:
