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
 * testOIBasic - ordered index test
 */

#include <ndb_global.h>

#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbTest.hpp>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <NdbThread.h>
#include <NdbTick.h>
#include <NdbSleep.h>
#include <my_sys.h>
#include <NdbSqlUtil.hpp>

// options

struct Opt {
  // common options
  unsigned m_batch;
  const char* m_bound;
  const char* m_case;
  bool m_collsp;
  bool m_core;
  const char* m_csname;
  CHARSET_INFO* m_cs;
  int m_die;
  bool m_dups;
  NdbDictionary::Object::FragmentType m_fragtype;
  unsigned m_subsubloop;
  const char* m_index;
  unsigned m_loop;
  bool m_msglock;
  bool m_nologging;
  bool m_noverify;
  unsigned m_pctnull;
  unsigned m_rows;
  unsigned m_samples;
  unsigned m_scanpar;
  unsigned m_scanstop;
  int m_seed;
  unsigned m_subloop;
  const char* m_table;
  unsigned m_threads;
  int m_v;      // int for lint
  Opt() :
    m_batch(32),
    m_bound("01234"),
    m_case(0),
    m_collsp(false),
    m_core(false),
    m_csname("random"),
    m_cs(0),
    m_die(0),
    m_dups(false),
    m_fragtype(NdbDictionary::Object::FragUndefined),
    m_subsubloop(4),
    m_index(0),
    m_loop(1),
    m_msglock(true),
    m_nologging(false),
    m_noverify(false),
    m_pctnull(10),
    m_rows(1000),
    m_samples(0),
    m_scanpar(0),
    m_scanstop(0),
    m_seed(-1),
    m_subloop(4),
    m_table(0),
    m_threads(4),
    m_v(1) {
  }
};

static Opt g_opt;

static void printcases();
static void printtables();

static void
printhelp()
{
  Opt d;
  ndbout
    << "usage: testOIbasic [options]" << endl
    << "  -batch N      pk operations in batch [" << d.m_batch << "]" << endl
    << "  -bound xyz    use only these bound types 0-4 [" << d.m_bound << "]" << endl
    << "  -case abc     only given test cases (letters a-z)" << endl
    << "  -collsp       use strnncollsp instead of strnxfrm" << endl
    << "  -core         core dump on error [" << d.m_core << "]" << endl
    << "  -csname S     charset or collation [" << d.m_csname << "]" << endl
    << "  -die nnn      exit immediately on NDB error code nnn" << endl
    << "  -dups         allow duplicate tuples from index scan [" << d.m_dups << "]" << endl
    << "  -fragtype T   fragment type single/small/medium/large" << endl
    << "  -index xyz    only given index numbers (digits 0-9)" << endl
    << "  -loop N       loop count full suite 0=forever [" << d.m_loop << "]" << endl
    << "  -nologging    create tables in no-logging mode" << endl
    << "  -noverify     skip index verifications" << endl
    << "  -pctnull N    pct NULL values in nullable column [" << d.m_pctnull << "]" << endl
    << "  -rows N       rows per thread [" << d.m_rows << "]" << endl
    << "  -samples N    samples for some timings (0=all) [" << d.m_samples << "]" << endl
    << "  -scanpar N    scan parallelism [" << d.m_scanpar << "]" << endl
    << "  -seed N       srandom seed 0=loop number -1=random [" << d.m_seed << "]" << endl
    << "  -subloop N    subtest loop count [" << d.m_subloop << "]" << endl
    << "  -table xyz    only given table numbers (digits 0-9)" << endl
    << "  -threads N    number of threads [" << d.m_threads << "]" << endl
    << "  -vN           verbosity [" << d.m_v << "]" << endl
    << "  -h or -help   print this help text" << endl
    ;
  printcases();
  printtables();
}

// not yet configurable
static const bool g_store_null_key = true;

// compare NULL like normal value (NULL < not NULL, NULL == NULL)
static const bool g_compare_null = true;

static const char* hexstr = "0123456789abcdef";

// random ints

static unsigned
urandom(unsigned n)
{
  if (n == 0)
    return 0;
  unsigned i = random() % n;
  return i;
}

static int
irandom(unsigned n)
{
  if (n == 0)
    return 0;
  int i = random() % n;
  if (random() & 0x1)
    i = -i;
  return i;
}

// log and error macros

static NdbMutex *ndbout_mutex = NULL;

static unsigned getthrno();

static const char*
getthrstr()
{
  static char buf[20];
  unsigned n = getthrno();
  if (n == (unsigned)-1)
    strcpy(buf, "");
  else {
    unsigned m =
      g_opt.m_threads < 10 ? 1 :
      g_opt.m_threads < 100 ? 2 : 3;
    sprintf(buf, "[%0*u] ", m, n);
  }
  return buf;
}

#define LLN(n, s) \
  do { \
    if ((n) > g_opt.m_v) break; \
    if (g_opt.m_msglock) NdbMutex_Lock(ndbout_mutex); \
    ndbout << getthrstr() << s << endl; \
    if (g_opt.m_msglock) NdbMutex_Unlock(ndbout_mutex); \
  } while(0)

#define LL0(s) LLN(0, s)
#define LL1(s) LLN(1, s)
#define LL2(s) LLN(2, s)
#define LL3(s) LLN(3, s)
#define LL4(s) LLN(4, s)
#define LL5(s) LLN(5, s)

// following check a condition and return -1 on failure

#undef CHK      // simple check
#undef CHKTRY   // check with action on fail
#undef CHKCON   // print NDB API errors on failure

#define CHK(x)  CHKTRY(x, ;)

#define CHKTRY(x, act) \
  do { \
    if (x) break; \
    LL0("line " << __LINE__ << ": " << #x << " failed"); \
    if (g_opt.m_core) abort(); \
    act; \
    return -1; \
  } while (0)

#define CHKCON(x, con) \
  do { \
    if (x) break; \
    LL0("line " << __LINE__ << ": " << #x << " failed"); \
    (con).printerror(ndbout); \
    if (g_opt.m_core) abort(); \
    return -1; \
  } while (0)

// method parameters

class Thr;
class Con;
class Tab;
class Set;
class Tmr;

struct Par : public Opt {
  unsigned m_no;
  Con* m_con;
  Con& con() const { assert(m_con != 0); return *m_con; }
  const Tab* m_tab;
  const Tab& tab() const { assert(m_tab != 0); return *m_tab; }
  Set* m_set;
  Set& set() const { assert(m_set != 0); return *m_set; }
  Tmr* m_tmr;
  Tmr& tmr() const { assert(m_tmr != 0); return *m_tmr; }
  unsigned m_lno;
  unsigned m_slno;
  unsigned m_totrows;
  // value calculation
  unsigned m_range;
  unsigned m_pctrange;
  unsigned m_pctbrange;
  int m_bdir;
  // choice of key
  bool m_randomkey;
  // do verify after read
  bool m_verify;
  // deadlock possible
  bool m_deadlock;
  NdbOperation::LockMode m_lockmode;
  // ordered range scan
  bool m_ordered;
  bool m_descending;
  // timer location
  Par(const Opt& opt) :
    Opt(opt),
    m_no(0),
    m_con(0),
    m_tab(0),
    m_set(0),
    m_tmr(0),
    m_lno(0),
    m_slno(0),
    m_totrows(m_threads * m_rows),
    m_range(m_rows),
    m_pctrange(40),
    m_pctbrange(80),
    m_bdir(0),
    m_randomkey(false),
    m_verify(false),
    m_deadlock(false),
    m_lockmode(NdbOperation::LM_Read),
    m_ordered(false),
    m_descending(false) {
  }
};

static bool
usetable(Par par, unsigned i)
{
  return par.m_table == 0 || strchr(par.m_table, '0' + i) != 0;
}

static bool
useindex(Par par, unsigned i)
{
  return par.m_index == 0 || strchr(par.m_index, '0' + i) != 0;
}

static unsigned
thrrow(Par par, unsigned j)
{
  return par.m_threads * j + par.m_no;
}

static bool
isthrrow(Par par, unsigned i)
{
  return i % par.m_threads == par.m_no;
}

// timer

struct Tmr {
  void clr();
  void on();
  void off(unsigned cnt = 0);
  const char* time();
  const char* pct(const Tmr& t1);
  const char* over(const Tmr& t1);
  NDB_TICKS m_on;
  unsigned m_ms;
  unsigned m_cnt;
  char m_time[100];
  char m_text[100];
  Tmr() { clr(); }
};

void
Tmr::clr()
{
  m_on = m_ms = m_cnt = m_time[0] = m_text[0] = 0;
}

void
Tmr::on()
{
  assert(m_on == 0);
  m_on = NdbTick_CurrentMillisecond();
}

void
Tmr::off(unsigned cnt)
{
  NDB_TICKS off = NdbTick_CurrentMillisecond();
  assert(m_on != 0 && off >= m_on);
  m_ms += off - m_on;
  m_cnt += cnt;
  m_on = 0;
}

const char*
Tmr::time()
{
  if (m_cnt == 0) {
    sprintf(m_time, "%u ms", m_ms);
  } else {
    sprintf(m_time, "%u ms per %u ( %u ms per 1000 )", m_ms, m_cnt, (1000 * m_ms) / m_cnt);
  }
  return m_time;
}

const char*
Tmr::pct(const Tmr& t1)
{
  if (0 < t1.m_ms) {
    sprintf(m_text, "%u pct", (100 * m_ms) / t1.m_ms);
  } else {
    sprintf(m_text, "[cannot measure]");
  }
  return m_text;
}

const char*
Tmr::over(const Tmr& t1)
{
  if (0 < t1.m_ms) {
    if (t1.m_ms <= m_ms)
      sprintf(m_text, "%u pct", (100 * (m_ms - t1.m_ms)) / t1.m_ms);
    else
      sprintf(m_text, "-%u pct", (100 * (t1.m_ms - m_ms)) / t1.m_ms);
  } else {
    sprintf(m_text, "[cannot measure]");
  }
  return m_text;
}

// list of ints

struct Lst {
  Lst();
  unsigned m_arr[1000];
  unsigned m_cnt;
  void push(unsigned i);
  unsigned cnt() const;
  void reset();
};

Lst::Lst() :
  m_cnt(0)
{
}

void
Lst::push(unsigned i)
{
  assert(m_cnt < sizeof(m_arr)/sizeof(m_arr[0]));
  m_arr[m_cnt++] = i;
}

unsigned
Lst::cnt() const
{
  return m_cnt;
}

void
Lst::reset()
{
  m_cnt = 0;
}

// character sets

static const unsigned maxcsnumber = 512;
static const unsigned maxcharcount = 32;
static const unsigned maxcharsize = 4;
static const unsigned maxxmulsize = 8;

// single mb char
struct Chr {
  unsigned char m_bytes[maxcharsize];
  unsigned char m_xbytes[maxxmulsize * maxcharsize];
  unsigned m_size;
  Chr();
};

Chr::Chr()
{
  memset(m_bytes, 0, sizeof(m_bytes));
  memset(m_xbytes, 0, sizeof(m_xbytes));
  m_size = 0;
}

// charset and random valid chars to use
struct Chs {
  CHARSET_INFO* m_cs;
  unsigned m_xmul;
  Chr* m_chr;
  Chs(CHARSET_INFO* cs);
  ~Chs();
};

static NdbOut&
operator<<(NdbOut& out, const Chs& chs);

Chs::Chs(CHARSET_INFO* cs) :
  m_cs(cs)
{
  m_xmul = m_cs->strxfrm_multiply;
  if (m_xmul == 0)
    m_xmul = 1;
  assert(m_xmul <= maxxmulsize);
  m_chr = new Chr [maxcharcount];
  unsigned i = 0;
  unsigned miss1 = 0;
  unsigned miss2 = 0;
  unsigned miss3 = 0;
  unsigned miss4 = 0;
  while (i < maxcharcount) {
    unsigned char* bytes = m_chr[i].m_bytes;
    unsigned char* xbytes = m_chr[i].m_xbytes;
    unsigned& size = m_chr[i].m_size;
    bool ok;
    size = m_cs->mbminlen + urandom(m_cs->mbmaxlen - m_cs->mbminlen + 1);
    assert(m_cs->mbminlen <= size && size <= m_cs->mbmaxlen);
    // prefer longer chars
    if (size == m_cs->mbminlen && m_cs->mbminlen < m_cs->mbmaxlen && urandom(5) != 0)
      continue;
    for (unsigned j = 0; j < size; j++) {
      bytes[j] = urandom(256);
    }
    int not_used;
    // check wellformed
    const char* sbytes = (const char*)bytes;
    if ((*cs->cset->well_formed_len)(cs, sbytes, sbytes + size, 1, &not_used) != size) {
      miss1++;
      continue;
    }
    // check no proper prefix wellformed
    ok = true;
    for (unsigned j = 1; j < size; j++) {
      if ((*cs->cset->well_formed_len)(cs, sbytes, sbytes + j, 1, &not_used) == j) {
        ok = false;
        break;
      }
    }
    if (! ok) {
      miss2++;
      continue;
    }
    // normalize
    memset(xbytes, 0, sizeof(xbytes));
    // currently returns buffer size always
    int xlen = (*cs->coll->strnxfrm)(cs, xbytes, m_xmul * size, bytes, size);
    // check we got something
    ok = false;
    for (unsigned j = 0; j < xlen; j++) {
      if (xbytes[j] != 0) {
        ok = true;
        break;
      }
    }
    if (! ok) {
      miss3++;
      continue;
    }
    // check for duplicate (before normalize)
    ok = true;
    for (unsigned j = 0; j < i; j++) {
      const Chr& chr = m_chr[j];
      if (chr.m_size == size && memcmp(chr.m_bytes, bytes, size) == 0) {
        ok = false;
        break;
      }
    }
    if (! ok) {
      miss4++;
      continue;
    }
    i++;
  }
  bool disorder = true;
  unsigned bubbles = 0;
  while (disorder) {
    disorder = false;
    for (unsigned i = 1; i < maxcharcount; i++) {
      unsigned len = sizeof(m_chr[i].m_xbytes);
      if (memcmp(m_chr[i-1].m_xbytes, m_chr[i].m_xbytes, len) > 0) {
        Chr chr = m_chr[i];
        m_chr[i] = m_chr[i-1];
        m_chr[i-1] = chr;
        disorder = true;
        bubbles++;
      }
    }
  }
  LL3("inited charset " << *this << " miss=" << miss1 << "," << miss2 << "," << miss3 << "," << miss4 << " bubbles=" << bubbles);
}

Chs::~Chs()
{
  delete [] m_chr;
}

static NdbOut&
operator<<(NdbOut& out, const Chs& chs)
{
  CHARSET_INFO* cs = chs.m_cs;
  out << cs->name << "[" << cs->mbminlen << "-" << cs->mbmaxlen << "," << chs.m_xmul << "]";
  return out;
}

static Chs* cslist[maxcsnumber];

static void
resetcslist()
{
  for (unsigned i = 0; i < maxcsnumber; i++) {
    delete cslist[i];
    cslist[i] = 0;
  }
}

static Chs*
getcs(Par par)
{
  CHARSET_INFO* cs;
  if (par.m_cs != 0) {
    cs = par.m_cs;
  } else {
    while (1) {
      unsigned n = urandom(maxcsnumber);
      cs = get_charset(n, MYF(0));
      if (cs != 0) {
        // prefer complex charsets
        if (cs->mbmaxlen != 1 || urandom(5) == 0)
          break;
      }
    }
  }
  if (cslist[cs->number] == 0)
    cslist[cs->number] = new Chs(cs);
  return cslist[cs->number];
}

// tables and indexes

// Col - table column

struct Col {
  enum Type {
    Unsigned = NdbDictionary::Column::Unsigned,
    Char = NdbDictionary::Column::Char,
    Varchar = NdbDictionary::Column::Varchar,
    Longvarchar = NdbDictionary::Column::Longvarchar
  };
  const class Tab& m_tab;
  unsigned m_num;
  const char* m_name;
  bool m_pk;
  Type m_type;
  unsigned m_length;
  unsigned m_bytelength;        // multiplied by char width
  unsigned m_attrsize;          // base type size
  unsigned m_headsize;          // length bytes
  unsigned m_bytesize;          // full value size
  bool m_nullable;
  const Chs* m_chs;
  Col(const class Tab& tab, unsigned num, const char* name, bool pk, Type type, unsigned length, bool nullable, const Chs* chs);
  ~Col();
  bool equal(const Col& col2) const;
  void wellformed(const void* addr) const;
};

Col::Col(const class Tab& tab, unsigned num, const char* name, bool pk, Type type, unsigned length, bool nullable, const Chs* chs) :
  m_tab(tab),
  m_num(num),
  m_name(strcpy(new char [strlen(name) + 1], name)),
  m_pk(pk),
  m_type(type),
  m_length(length),
  m_bytelength(length * (chs == 0 ? 1 : chs->m_cs->mbmaxlen)),
  m_attrsize(
      type == Unsigned ? sizeof(Uint32) :
      type == Char ? sizeof(char) :
      type == Varchar ? sizeof(char) :
      type == Longvarchar ? sizeof(char) : ~0),
  m_headsize(
      type == Unsigned ? 0 :
      type == Char ? 0 :
      type == Varchar ? 1 :
      type == Longvarchar ? 2 : ~0),
  m_bytesize(m_headsize + m_attrsize * m_bytelength),
  m_nullable(nullable),
  m_chs(chs)
{
  // fix long varchar
  if (type == Varchar && m_bytelength > 255) {
    m_type = Longvarchar;
    m_headsize += 1;
    m_bytesize += 1;
  }
}

Col::~Col()
{
  delete [] m_name;
}

bool
Col::equal(const Col& col2) const
{
  return m_type == col2.m_type && m_length == col2.m_length && m_chs == col2.m_chs;
}

void
Col::wellformed(const void* addr) const
{
  switch (m_type) {
  case Col::Unsigned:
    break;
  case Col::Char:
    {
      CHARSET_INFO* cs = m_chs->m_cs;
      const char* src = (const char*)addr;
      unsigned len = m_bytelength;
      int not_used;
      assert((*cs->cset->well_formed_len)(cs, src, src + len, 0xffff, &not_used) == len);
    }
    break;
  case Col::Varchar:
    {
      CHARSET_INFO* cs = m_chs->m_cs;
      const unsigned char* src = (const unsigned char*)addr;
      const char* ssrc = (const char*)src;
      unsigned len = src[0];
      int not_used;
      assert(len <= m_bytelength);
      assert((*cs->cset->well_formed_len)(cs, ssrc + 1, ssrc + 1 + len, 0xffff, &not_used) == len);
    }
    break;
  case Col::Longvarchar:
    {
      CHARSET_INFO* cs = m_chs->m_cs;
      const unsigned char* src = (const unsigned char*)addr;
      const char* ssrc = (const char*)src;
      unsigned len = src[0] + (src[1] << 8);
      int not_used;
      assert(len <= m_bytelength);
      assert((*cs->cset->well_formed_len)(cs, ssrc + 2, ssrc + 2 + len, 0xffff, &not_used) == len);
    }
    break;
  default:
    assert(false);
    break;
  }
}

static NdbOut&
operator<<(NdbOut& out, const Col& col)
{
  out << "col[" << col.m_num << "] " << col.m_name;
  switch (col.m_type) {
  case Col::Unsigned:
    out << " unsigned";
    break;
  case Col::Char:
    {
      CHARSET_INFO* cs = col.m_chs->m_cs;
      out << " char(" << col.m_length << "*" << cs->mbmaxlen << ";" << cs->name << ")";
    }
    break;
  case Col::Varchar:
    {
      CHARSET_INFO* cs = col.m_chs->m_cs;
      out << " varchar(" << col.m_length << "*" << cs->mbmaxlen << ";" << cs->name << ")";
    }
    break;
  case Col::Longvarchar:
    {
      CHARSET_INFO* cs = col.m_chs->m_cs;
      out << " longvarchar(" << col.m_length << "*" << cs->mbmaxlen << ";" << cs->name << ")";
    }
    break;
  default:
    out << "type" << (int)col.m_type;
    assert(false);
    break;
  }
  out << (col.m_pk ? " pk" : "");
  out << (col.m_nullable ? " nullable" : "");
  return out;
}

// ICol - index column

struct ICol {
  const class ITab& m_itab;
  unsigned m_num;
  const Col& m_col;
  ICol(const class ITab& itab, unsigned num, const Col& col);
  ~ICol();
};

ICol::ICol(const class ITab& itab, unsigned num, const Col& col) :
  m_itab(itab),
  m_num(num),
  m_col(col)
{
}

ICol::~ICol()
{
}

static NdbOut&
operator<<(NdbOut& out, const ICol& icol)
{
  out << "icol[" << icol.m_num << "] " << icol.m_col;
  return out;
}

// ITab - index

struct ITab {
  enum Type {
    OrderedIndex = NdbDictionary::Index::OrderedIndex,
    UniqueHashIndex = NdbDictionary::Index::UniqueHashIndex
  };
  const class Tab& m_tab;
  const char* m_name;
  Type m_type;
  unsigned m_icols;
  const ICol** m_icol;
  unsigned m_colmask;
  ITab(const class Tab& tab, const char* name, Type type, unsigned icols);
  ~ITab();
  void icoladd(unsigned k, const ICol* icolptr);
};

ITab::ITab(const class Tab& tab, const char* name, Type type, unsigned icols) :
  m_tab(tab),
  m_name(strcpy(new char [strlen(name) + 1], name)),
  m_type(type),
  m_icols(icols),
  m_icol(new const ICol* [icols + 1]),
  m_colmask(0)
{
  for (unsigned k = 0; k <= m_icols; k++)
    m_icol[k] = 0;
}

ITab::~ITab()
{
  delete [] m_name;
  for (unsigned i = 0; i < m_icols; i++)
    delete m_icol[i];
  delete [] m_icol;
}

void
ITab::icoladd(unsigned k, const ICol* icolptr)
{
  assert(k == icolptr->m_num && k < m_icols && m_icol[k] == 0);
  m_icol[k] = icolptr;
  m_colmask |= (1 << icolptr->m_col.m_num);
}

static NdbOut&
operator<<(NdbOut& out, const ITab& itab)
{
  out << "itab " << itab.m_name << " icols=" << itab.m_icols;
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const ICol& icol = *itab.m_icol[k];
    out << endl << icol;
  }
  return out;
}

// Tab - table

struct Tab {
  const char* m_name;
  unsigned m_cols;
  const Col** m_col;
  unsigned m_itabs;
  const ITab** m_itab;
  // pk must contain an Unsigned column
  unsigned m_keycol;
  void coladd(unsigned k, Col* colptr);
  void itabadd(unsigned j, ITab* itab);
  Tab(const char* name, unsigned cols, unsigned itabs, unsigned keycol);
  ~Tab();
};

Tab::Tab(const char* name, unsigned cols, unsigned itabs, unsigned keycol) :
  m_name(strcpy(new char [strlen(name) + 1], name)),
  m_cols(cols),
  m_col(new const Col* [cols + 1]),
  m_itabs(itabs),
  m_itab(new const ITab* [itabs + 1]),
  m_keycol(keycol)
{
  for (unsigned k = 0; k <= cols; k++)
    m_col[k] = 0;
  for (unsigned j = 0; j <= itabs; j++)
    m_itab[j] = 0;
}

Tab::~Tab()
{
  delete [] m_name;
  for (unsigned i = 0; i < m_cols; i++)
    delete m_col[i];
  delete [] m_col;
  for (unsigned i = 0; i < m_itabs; i++)
    delete m_itab[i];
  delete [] m_itab;
}

void
Tab::coladd(unsigned k, Col* colptr)
{
  assert(k == colptr->m_num && k < m_cols && m_col[k] == 0);
  m_col[k] = colptr;
}

void
Tab::itabadd(unsigned j, ITab* itabptr)
{
  assert(j < m_itabs && m_itab[j] == 0);
  m_itab[j] = itabptr;
}

static NdbOut&
operator<<(NdbOut& out, const Tab& tab)
{
  out << "tab " << tab.m_name << " cols=" << tab.m_cols;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Col& col =  *tab.m_col[k];
    out << endl << col;
  }
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0)
      continue;
    const ITab& itab = *tab.m_itab[i];
    out << endl << itab;
  }
  return out;
}

// make table structs

static const Tab** tablist = 0;
static unsigned tabcount = 0;

static void
verifytables()
{
  for (unsigned j = 0; j < tabcount; j++) {
    const Tab* t = tablist[j];
    if (t == 0)
      continue;
    assert(t->m_cols != 0 && t->m_col != 0);
    for (unsigned k = 0; k < t->m_cols; k++) {
      const Col* c = t->m_col[k];
      assert(c != 0 && c->m_num == k);
      assert(! (c->m_pk && c->m_nullable));
    }
    assert(t->m_col[t->m_cols] == 0);
    {
      assert(t->m_keycol < t->m_cols);
      const Col* c = t->m_col[t->m_keycol];
      assert(c->m_pk && c->m_type == Col::Unsigned);
    }
    assert(t->m_itabs != 0 && t->m_itab != 0);
    for (unsigned i = 0; i < t->m_itabs; i++) {
      const ITab* x = t->m_itab[i];
      if (x == 0)
        continue;
      assert(x != 0 && x->m_icols != 0 && x->m_icol != 0);
      for (unsigned k = 0; k < x->m_icols; k++) {
        const ICol* c = x->m_icol[k];
        assert(c != 0 && c->m_num == k && c->m_col.m_num < t->m_cols);
        if (x->m_type == ITab::UniqueHashIndex) {
          assert(! c->m_col.m_nullable);
        }
      }
    }
    assert(t->m_itab[t->m_itabs] == 0);
  }
}

static void
makebuiltintables(Par par)
{
  LL2("makebuiltintables");
  resetcslist();
  tabcount = 3;
  if (tablist == 0) {
    tablist = new const Tab* [tabcount];
    for (unsigned j = 0; j < tabcount; j++) {
      tablist[j] = 0;
    }
  } else {
    for (unsigned j = 0; j < tabcount; j++) {
      delete tablist[j];
      tablist[j] = 0;
    }
  }
  // ti0 - basic
  if (usetable(par, 0)) {
    Tab* t = new Tab("ti0", 5, 7, 0);
    // name - pk - type - length - nullable - cs
    t->coladd(0, new Col(*t, 0, "a", 1, Col::Unsigned, 1, 0, 0));
    t->coladd(1, new Col(*t, 1, "b", 0, Col::Unsigned, 1, 1, 0));
    t->coladd(2, new Col(*t, 2, "c", 0, Col::Unsigned, 1, 0, 0));
    t->coladd(3, new Col(*t, 3, "d", 0, Col::Unsigned, 1, 1, 0));
    t->coladd(4, new Col(*t, 4, "e", 0, Col::Unsigned, 1, 0, 0));
    if (useindex(par, 0)) {
      // a
      ITab* x = new ITab(*t, "ti0x0", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      t->itabadd(0, x);
    }
    if (useindex(par, 1)) {
      // b
      ITab* x = new ITab(*t, "ti0x1", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      t->itabadd(1, x);
    }
    if (useindex(par, 2)) {
      // b, c
      ITab* x = new ITab(*t, "ti0x2", ITab::OrderedIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      t->itabadd(2, x);
    }
    if (useindex(par, 3)) {
      // b, e, c, d
      ITab* x = new ITab(*t, "ti0x3", ITab::OrderedIndex, 4);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[4]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[2]));
      x->icoladd(3, new ICol(*x, 3, *t->m_col[3]));
      t->itabadd(3, x);
    }
    if (useindex(par, 4)) {
      // a, c
      ITab* x = new ITab(*t, "ti0z4", ITab::UniqueHashIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      t->itabadd(4, x);
    }
    if (useindex(par, 5)) {
      // a, e
      ITab* x = new ITab(*t, "ti0z5", ITab::UniqueHashIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[4]));
      t->itabadd(5, x);
    }
    tablist[0] = t;
  }
  // ti1 - simple char fields
  if (usetable(par, 1)) {
    Tab* t = new Tab("ti1", 5, 7, 1);
    // name - pk - type - length - nullable - cs
    t->coladd(0, new Col(*t, 0, "a", 0, Col::Unsigned, 1, 0, 0));
    t->coladd(1, new Col(*t, 1, "b", 1, Col::Unsigned, 1, 0, 0));
    t->coladd(2, new Col(*t, 2, "c", 0, Col::Char, 20, 1, getcs(par)));
    t->coladd(3, new Col(*t, 3, "d", 0, Col::Varchar, 5, 0, getcs(par)));
    t->coladd(4, new Col(*t, 4, "e", 0, Col::Longvarchar, 5, 1, getcs(par)));
    if (useindex(par, 0)) {
      // b
      ITab* x = new ITab(*t, "ti1x0", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      t->itabadd(0, x);
    }
    if (useindex(par, 1)) {
      // c, a
      ITab* x = new ITab(*t, "ti1x1", ITab::OrderedIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[2]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[0]));
      t->itabadd(1, x);
    }
    if (useindex(par, 2)) {
      // d
      ITab* x = new ITab(*t, "ti1x2", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[3]));
      t->itabadd(2, x);
    }
    if (useindex(par, 3)) {
      // e, d, c, b
      ITab* x = new ITab(*t, "ti1x3", ITab::OrderedIndex, 4);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[4]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[3]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[2]));
      x->icoladd(3, new ICol(*x, 3, *t->m_col[1]));
      t->itabadd(3, x);
    }
    if (useindex(par, 4)) {
      // a, b
      ITab* x = new ITab(*t, "ti1z4", ITab::UniqueHashIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[1]));
      t->itabadd(4, x);
    }
    if (useindex(par, 5)) {
      // a, b, d
      ITab* x = new ITab(*t, "ti1z5", ITab::UniqueHashIndex, 3);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[1]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[3]));
      t->itabadd(5, x);
    }
    tablist[1] = t;
  }
  // ti2 - complex char fields
  if (usetable(par, 2)) {
    Tab* t = new Tab("ti2", 5, 7, 2);
    // name - pk - type - length - nullable - cs
    t->coladd(0, new Col(*t, 0, "a", 1, Col::Char, 31, 0, getcs(par)));
    t->coladd(1, new Col(*t, 1, "b", 0, Col::Char, 4, 1, getcs(par)));
    t->coladd(2, new Col(*t, 2, "c", 1, Col::Unsigned, 1, 0, 0));
    t->coladd(3, new Col(*t, 3, "d", 1, Col::Varchar, 128, 0, getcs(par)));
    t->coladd(4, new Col(*t, 4, "e", 0, Col::Varchar, 7, 0, getcs(par)));
    if (useindex(par, 0)) {
      // a, c, d
      ITab* x = new ITab(*t, "ti2x0", ITab::OrderedIndex, 3);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[3]));
      t->itabadd(0, x);
    }
    if (useindex(par, 1)) {
      // e, d, c, b, a
      ITab* x = new ITab(*t, "ti2x1", ITab::OrderedIndex, 5);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[4]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[3]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[2]));
      x->icoladd(3, new ICol(*x, 3, *t->m_col[1]));
      x->icoladd(4, new ICol(*x, 4, *t->m_col[0]));
      t->itabadd(1, x);
    }
    if (useindex(par, 2)) {
      // d
      ITab* x = new ITab(*t, "ti2x2", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[3]));
      t->itabadd(2, x);
    }
    if (useindex(par, 3)) {
      // b
      ITab* x = new ITab(*t, "ti2x3", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      t->itabadd(3, x);
    }
    if (useindex(par, 4)) {
      // a, c
      ITab* x = new ITab(*t, "ti2z4", ITab::UniqueHashIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      t->itabadd(4, x);
    }
    if (useindex(par, 5)) {
      // a, c, d, e
      ITab* x = new ITab(*t, "ti2z5", ITab::UniqueHashIndex, 4);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[3]));
      x->icoladd(3, new ICol(*x, 3, *t->m_col[4]));
      t->itabadd(5, x);
    }
    tablist[2] = t;
  }
  verifytables();
}

// connections

static Ndb_cluster_connection* g_ncc = 0;

struct Con {
  Ndb* m_ndb;
  NdbDictionary::Dictionary* m_dic;
  NdbConnection* m_tx;
  NdbOperation* m_op;
  NdbIndexOperation* m_indexop;
  NdbScanOperation* m_scanop;
  NdbIndexScanOperation* m_indexscanop;
  NdbScanFilter* m_scanfilter;
  enum ScanMode { ScanNo = 0, Committed, Latest, Exclusive };
  ScanMode m_scanmode;
  enum ErrType { ErrNone = 0, ErrDeadlock, ErrOther };
  ErrType m_errtype;
  Con() :
    m_ndb(0), m_dic(0), m_tx(0), m_op(0), m_indexop(0),
    m_scanop(0), m_indexscanop(0), m_scanfilter(0),
    m_scanmode(ScanNo), m_errtype(ErrNone) {}
  ~Con() {
    if (m_tx != 0)
      closeTransaction();
  }
  int connect();
  void connect(const Con& con);
  void disconnect();
  int startTransaction();
  int getNdbOperation(const Tab& tab);
  int getNdbIndexOperation1(const ITab& itab, const Tab& tab);
  int getNdbIndexOperation(const ITab& itab, const Tab& tab);
  int getNdbScanOperation(const Tab& tab);
  int getNdbIndexScanOperation1(const ITab& itab, const Tab& tab);
  int getNdbIndexScanOperation(const ITab& itab, const Tab& tab);
  int getNdbScanFilter();
  int equal(int num, const char* addr);
  int getValue(int num, NdbRecAttr*& rec);
  int setValue(int num, const char* addr);
  int setBound(int num, int type, const void* value);
  int beginFilter(int group);
  int endFilter();
  int setFilter(int num, int cond, const void* value, unsigned len);
  int execute(ExecType t);
  int execute(ExecType t, bool& deadlock);
  int readTuples(Par par);
  int readIndexTuples(Par par);
  int executeScan();
  int nextScanResult(bool fetchAllowed);
  int nextScanResult(bool fetchAllowed, bool& deadlock);
  int updateScanTuple(Con& con2);
  int deleteScanTuple(Con& con2);
  void closeScan();
  void closeTransaction();
  void printerror(NdbOut& out);
};

int
Con::connect()
{
  assert(m_ndb == 0);
  m_ndb = new Ndb(g_ncc, "TEST_DB");
  CHKCON(m_ndb->init() == 0, *this);
  CHKCON(m_ndb->waitUntilReady(30) == 0, *this);
  m_tx = 0, m_op = 0;
  return 0;
}

void
Con::connect(const Con& con)
{
  assert(m_ndb == 0);
  m_ndb = con.m_ndb;
}

void
Con::disconnect()
{
  delete m_ndb;
  m_ndb = 0, m_dic = 0, m_tx = 0, m_op = 0;
}

int
Con::startTransaction()
{
  assert(m_ndb != 0);
  if (m_tx != 0)
    closeTransaction();
  CHKCON((m_tx = m_ndb->startTransaction()) != 0, *this);
  return 0;
}

int
Con::getNdbOperation(const Tab& tab)
{
  assert(m_tx != 0);
  CHKCON((m_op = m_tx->getNdbOperation(tab.m_name)) != 0, *this);
  return 0;
}

int
Con::getNdbIndexOperation1(const ITab& itab, const Tab& tab)
{
  assert(m_tx != 0);
  CHKCON((m_op = m_indexop = m_tx->getNdbIndexOperation(itab.m_name, tab.m_name)) != 0, *this);
  return 0;
}

int
Con::getNdbIndexOperation(const ITab& itab, const Tab& tab)
{
  assert(m_tx != 0);
  unsigned tries = 0;
  while (1) {
    if (getNdbIndexOperation1(itab, tab) == 0)
      break;
    CHK(++tries < 10);
    NdbSleep_MilliSleep(100);
  }
  return 0;
}

int
Con::getNdbScanOperation(const Tab& tab)
{
  assert(m_tx != 0);
  CHKCON((m_op = m_scanop = m_tx->getNdbScanOperation(tab.m_name)) != 0, *this);
  return 0;
}

int
Con::getNdbIndexScanOperation1(const ITab& itab, const Tab& tab)
{
  assert(m_tx != 0);
  CHKCON((m_op = m_scanop = m_indexscanop = m_tx->getNdbIndexScanOperation(itab.m_name, tab.m_name)) != 0, *this);
  return 0;
}

int
Con::getNdbIndexScanOperation(const ITab& itab, const Tab& tab)
{
  assert(m_tx != 0);
  unsigned tries = 0;
  while (1) {
    if (getNdbIndexScanOperation1(itab, tab) == 0)
      break;
    CHK(++tries < 10);
    NdbSleep_MilliSleep(100);
  }
  return 0;
}

int
Con::getNdbScanFilter()
{
  assert(m_tx != 0 && m_scanop != 0);
  delete m_scanfilter;
  m_scanfilter = new NdbScanFilter(m_scanop);
  return 0;
}

int
Con::equal(int num, const char* addr)
{
  assert(m_tx != 0 && m_op != 0);
  CHKCON(m_op->equal(num, addr) == 0, *this);
  return 0;
}

int
Con::getValue(int num, NdbRecAttr*& rec)
{
  assert(m_tx != 0 && m_op != 0);
  CHKCON((rec = m_op->getValue(num, 0)) != 0, *this);
  return 0;
}

int
Con::setValue(int num, const char* addr)
{
  assert(m_tx != 0 && m_op != 0);
  CHKCON(m_op->setValue(num, addr) == 0, *this);
  return 0;
}

int
Con::setBound(int num, int type, const void* value)
{
  assert(m_tx != 0 && m_indexscanop != 0);
  CHKCON(m_indexscanop->setBound(num, type, value) == 0, *this);
  return 0;
}

int
Con::beginFilter(int group)
{
  assert(m_tx != 0 && m_scanfilter != 0);
  CHKCON(m_scanfilter->begin((NdbScanFilter::Group)group) == 0, *this);
  return 0;
}

int
Con::endFilter()
{
  assert(m_tx != 0 && m_scanfilter != 0);
  CHKCON(m_scanfilter->end() == 0, *this);
  return 0;
}

int
Con::setFilter(int num, int cond, const void* value, unsigned len)
{
  assert(m_tx != 0 && m_scanfilter != 0);
  CHKCON(m_scanfilter->cmp((NdbScanFilter::BinaryCondition)cond, num, value, len) == 0, *this);
  return 0;
}

int
Con::execute(ExecType t)
{
  assert(m_tx != 0);
  CHKCON(m_tx->execute(t) == 0, *this);
  return 0;
}

int
Con::execute(ExecType t, bool& deadlock)
{
  int ret = execute(t);
  if (ret != 0) {
    if (deadlock && m_errtype == ErrDeadlock) {
      LL3("caught deadlock");
      ret = 0;
    }
  } else {
    deadlock = false;
  }
  CHK(ret == 0);
  return 0;
}

int
Con::readTuples(Par par)
{
  assert(m_tx != 0 && m_scanop != 0);
  CHKCON(m_scanop->readTuples(par.m_lockmode, 0, par.m_scanpar) == 0, *this);
  return 0;
}

int
Con::readIndexTuples(Par par)
{
  assert(m_tx != 0 && m_indexscanop != 0);
  CHKCON(m_indexscanop->readTuples(par.m_lockmode, 0, par.m_scanpar, par.m_ordered, par.m_descending) == 0, *this);
  return 0;
}

int
Con::executeScan()
{
  CHKCON(m_tx->execute(NoCommit) == 0, *this);
  return 0;
}

int
Con::nextScanResult(bool fetchAllowed)
{
  int ret;
  assert(m_scanop != 0);
  CHKCON((ret = m_scanop->nextResult(fetchAllowed)) != -1, *this);
  assert(ret == 0 || ret == 1 || (! fetchAllowed && ret == 2));
  return ret;
}

int
Con::nextScanResult(bool fetchAllowed, bool& deadlock)
{
  int ret = nextScanResult(fetchAllowed);
  if (ret == -1) {
    if (deadlock && m_errtype == ErrDeadlock) {
      LL3("caught deadlock");
      ret = 0;
    }
  } else {
    deadlock = false;
  }
  CHK(ret == 0 || ret == 1 || (! fetchAllowed && ret == 2));
  return ret;
}

int
Con::updateScanTuple(Con& con2)
{
  assert(con2.m_tx != 0);
  CHKCON((con2.m_op = m_scanop->updateCurrentTuple(con2.m_tx)) != 0, *this);
  return 0;
}

int
Con::deleteScanTuple(Con& con2)
{
  assert(con2.m_tx != 0);
  CHKCON(m_scanop->deleteCurrentTuple(con2.m_tx) == 0, *this);
  return 0;
}

void
Con::closeScan()
{
  assert(m_scanop != 0);
  m_scanop->close();
  m_scanop = 0, m_indexscanop = 0;

}

void
Con::closeTransaction()
{
  assert(m_ndb != 0 && m_tx != 0);
  m_ndb->closeTransaction(m_tx);
  m_tx = 0, m_op = 0;
  m_scanop = 0, m_indexscanop = 0;
}

void
Con::printerror(NdbOut& out)
{
  m_errtype = ErrOther;
  unsigned any = 0;
  int code;
  int die = 0;
  if (m_ndb) {
    if ((code = m_ndb->getNdbError().code) != 0) {
      LL0(++any << " ndb: error " << m_ndb->getNdbError());
      die += (code == g_opt.m_die);
    }
    if (m_dic && (code = m_dic->getNdbError().code) != 0) {
      LL0(++any << " dic: error " << m_dic->getNdbError());
      die += (code == g_opt.m_die);
    }
    if (m_tx) {
      if ((code = m_tx->getNdbError().code) != 0) {
        LL0(++any << " con: error " << m_tx->getNdbError());
        die += (code == g_opt.m_die);
        // 631 is new, occurs only on 4 db nodes, needs to be checked out
        if (code == 266 || code == 274 || code == 296 || code == 297 || code == 499 || code == 631)
          m_errtype = ErrDeadlock;
      }
      if (m_op && m_op->getNdbError().code != 0) {
        LL0(++any << " op : error " << m_op->getNdbError());
        die += (code == g_opt.m_die);
      }
    }
  }
  if (! any) {
    LL0("failed but no NDB error code");
  }
  if (die) {
    if (g_opt.m_core)
      abort();
    exit(1);
  }
}

// dictionary operations

static int
invalidateindex(Par par, const ITab& itab)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  con.m_ndb->getDictionary()->invalidateIndex(itab.m_name, tab.m_name);
  return 0;
}

static int
invalidateindex(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0)
      continue;
    const ITab& itab = *tab.m_itab[i];
    invalidateindex(par, itab);
  }
  return 0;
}

static int
invalidatetable(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  invalidateindex(par);
  con.m_ndb->getDictionary()->invalidateTable(tab.m_name);
  return 0;
}

static int
droptable(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  con.m_dic = con.m_ndb->getDictionary();
  if (con.m_dic->getTable(tab.m_name) == 0) {
    // how to check for error
    LL4("no table " << tab.m_name);
  } else {
    LL3("drop table " << tab.m_name);
    CHKCON(con.m_dic->dropTable(tab.m_name) == 0, con);
  }
  con.m_dic = 0;
  return 0;
}

static int
createtable(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  LL3("create table " << tab.m_name);
  LL4(tab);
  NdbDictionary::Table t(tab.m_name);
  if (par.m_fragtype != NdbDictionary::Object::FragUndefined) {
    t.setFragmentType(par.m_fragtype);
  }
  if (par.m_nologging) {
    t.setLogging(false);
  }
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Col& col = *tab.m_col[k];
    NdbDictionary::Column c(col.m_name);
    c.setType((NdbDictionary::Column::Type)col.m_type);
    c.setLength(col.m_bytelength); // for char NDB API uses length in bytes
    c.setPrimaryKey(col.m_pk);
    c.setNullable(col.m_nullable);
    if (col.m_chs != 0)
        c.setCharset(col.m_chs->m_cs);
    t.addColumn(c);
  }
  con.m_dic = con.m_ndb->getDictionary();
  CHKCON(con.m_dic->createTable(t) == 0, con);
  con.m_dic = 0;
  return 0;
}

static int
dropindex(Par par, const ITab& itab)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  con.m_dic = con.m_ndb->getDictionary();
  if (con.m_dic->getIndex(itab.m_name, tab.m_name) == 0) {
    // how to check for error
    LL4("no index " << itab.m_name);
  } else {
    LL3("drop index " << itab.m_name);
    CHKCON(con.m_dic->dropIndex(itab.m_name, tab.m_name) == 0, con);
  }
  con.m_dic = 0;
  return 0;
}

static int
dropindex(Par par)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0)
      continue;
    const ITab& itab = *tab.m_itab[i];
    CHK(dropindex(par, itab) == 0);
  }
  return 0;
}

static int
createindex(Par par, const ITab& itab)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  LL3("create index " << itab.m_name);
  LL4(itab);
  NdbDictionary::Index x(itab.m_name);
  x.setTable(tab.m_name);
  x.setType((NdbDictionary::Index::Type)itab.m_type);
  if (par.m_nologging || itab.m_type == ITab::OrderedIndex) {
    x.setLogging(false);
  }
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const ICol& icol = *itab.m_icol[k];
    const Col& col = icol.m_col;
    x.addColumnName(col.m_name);
  }
  con.m_dic = con.m_ndb->getDictionary();
  CHKCON(con.m_dic->createIndex(x) == 0, con);
  con.m_dic = 0;
  return 0;
}

static int
createindex(Par par)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0)
      continue;
    const ITab& itab = *tab.m_itab[i];
    CHK(createindex(par, itab) == 0);
  }
  return 0;
}

// data sets

// Val - typed column value

struct Val {
  const Col& m_col;
  union {
  Uint32 m_uint32;
  unsigned char* m_char;
  unsigned char* m_varchar;
  unsigned char* m_longvarchar;
  };
  Val(const Col& col);
  ~Val();
  void copy(const Val& val2);
  void copy(const void* addr);
  const void* dataaddr() const;
  bool m_null;
  int equal(Par par) const;
  int equal(Par par, const ICol& icol) const;
  int setval(Par par) const;
  void calc(Par par, unsigned i);
  void calckey(Par par, unsigned i);
  void calckeychars(Par par, unsigned i, unsigned& n, unsigned char* buf);
  void calcnokey(Par par);
  void calcnokeychars(Par par, unsigned& n, unsigned char* buf);
  int verify(Par par, const Val& val2) const;
  int cmp(Par par, const Val& val2) const;
  int cmpchars(Par par, const unsigned char* buf1, unsigned len1, const unsigned char* buf2, unsigned len2) const;
private:
  Val& operator=(const Val& val2);
};

static NdbOut&
operator<<(NdbOut& out, const Val& val);

Val::Val(const Col& col) :
  m_col(col)
{
  switch (col.m_type) {
  case Col::Unsigned:
    break;
  case Col::Char:
    m_char = new unsigned char [col.m_bytelength];
    break;
  case Col::Varchar:
    m_varchar = new unsigned char [1 + col.m_bytelength];
    break;
  case Col::Longvarchar:
    m_longvarchar = new unsigned char [2 + col.m_bytelength];
    break;
  default:
    assert(false);
    break;
  }
}

Val::~Val()
{
  const Col& col = m_col;
  switch (col.m_type) {
  case Col::Unsigned:
    break;
  case Col::Char:
    delete [] m_char;
    break;
  case Col::Varchar:
    delete [] m_varchar;
    break;
  case Col::Longvarchar:
    delete [] m_longvarchar;
    break;
  default:
    assert(false);
    break;
  }
}

void
Val::copy(const Val& val2)
{
  const Col& col = m_col;
  const Col& col2 = val2.m_col;
  assert(col.m_type == col2.m_type && col.m_length == col2.m_length);
  if (val2.m_null) {
    m_null = true;
    return;
  }
  copy(val2.dataaddr());
}

void
Val::copy(const void* addr)
{
  const Col& col = m_col;
  switch (col.m_type) {
  case Col::Unsigned:
    m_uint32 = *(const Uint32*)addr;
    break;
  case Col::Char:
    memcpy(m_char, addr, col.m_bytelength);
    break;
  case Col::Varchar:
    memcpy(m_varchar, addr, 1 + col.m_bytelength);
    break;
  case Col::Longvarchar:
    memcpy(m_longvarchar, addr, 2 + col.m_bytelength);
    break;
  default:
    assert(false);
    break;
  }
  m_null = false;
}

const void*
Val::dataaddr() const
{
  const Col& col = m_col;
  switch (col.m_type) {
  case Col::Unsigned:
    return &m_uint32;
  case Col::Char:
    return m_char;
  case Col::Varchar:
    return m_varchar;
  case Col::Longvarchar:
    return m_longvarchar;
  default:
    break;
  }
  assert(false);
  return 0;
}

int
Val::equal(Par par) const
{
  Con& con = par.con();
  const Col& col = m_col;
  assert(col.m_pk && ! m_null);
  const char* addr = (const char*)dataaddr();
  LL5("equal [" << col << "] " << *this);
  CHK(con.equal(col.m_num, addr) == 0);
  return 0;
}

int
Val::equal(Par par, const ICol& icol) const
{
  Con& con = par.con();
  assert(! m_null);
  const char* addr = (const char*)dataaddr();
  LL5("equal [" << icol << "] " << *this);
  CHK(con.equal(icol.m_num, addr) == 0);
  return 0;
}

int
Val::setval(Par par) const
{
  Con& con = par.con();
  const Col& col = m_col;
  assert(! col.m_pk);
  const char* addr = ! m_null ? (const char*)dataaddr() : 0;
  LL5("setval [" << col << "] " << *this);
  CHK(con.setValue(col.m_num, addr) == 0);
  return 0;
}

void
Val::calc(Par par, unsigned i)
{
  const Col& col = m_col;
  col.m_pk ? calckey(par, i) : calcnokey(par);
  if (! m_null)
    col.wellformed(dataaddr());
}

void
Val::calckey(Par par, unsigned i)
{
  const Col& col = m_col;
  m_null = false;
  switch (col.m_type) {
  case Col::Unsigned:
    m_uint32 = i;
    break;
  case Col::Char:
    {
      const Chs* chs = col.m_chs;
      CHARSET_INFO* cs = chs->m_cs;
      unsigned n = 0;
      calckeychars(par, i, n, m_char);
      // extend by appropriate space
      (*cs->cset->fill)(cs, (char*)&m_char[n], col.m_bytelength - n, 0x20);
    }
    break;
  case Col::Varchar:
    {
      unsigned n = 0;
      calckeychars(par, i, n, m_varchar + 1);
      // set length and pad with nulls
      m_varchar[0] = n;
      memset(&m_varchar[1 + n], 0, col.m_bytelength - n);
    }
    break;
  case Col::Longvarchar:
    {
      unsigned n = 0;
      calckeychars(par, i, n, m_longvarchar + 2);
      // set length and pad with nulls
      m_longvarchar[0] = (n & 0xff);
      m_longvarchar[1] = (n >> 8);
      memset(&m_longvarchar[2 + n], 0, col.m_bytelength - n);
    }
    break;
  default:
    assert(false);
    break;
  }
}

void
Val::calckeychars(Par par, unsigned i, unsigned& n, unsigned char* buf)
{
  const Col& col = m_col;
  const Chs* chs = col.m_chs;
  CHARSET_INFO* cs = chs->m_cs;
  n = 0;
  unsigned len = 0;
  while (len < col.m_length) {
    if (i % (1 + n) == 0) {
      break;
    }
    const Chr& chr = chs->m_chr[i % maxcharcount];
    assert(n + chr.m_size <= col.m_bytelength);
    memcpy(buf + n, chr.m_bytes, chr.m_size);
    n += chr.m_size;
    len++;
  }
}

void
Val::calcnokey(Par par)
{
  const Col& col = m_col;
  m_null = false;
  if (col.m_nullable && urandom(100) < par.m_pctnull) {
    m_null = true;
    return;
  }
  int r = irandom((par.m_pctrange * par.m_range) / 100);
  if (par.m_bdir != 0 && urandom(10) != 0) {
    if (r < 0 && par.m_bdir > 0 || r > 0 && par.m_bdir < 0)
      r = -r;
  }
  unsigned v = par.m_range + r;
  switch (col.m_type) {
  case Col::Unsigned:
    m_uint32 = v;
    break;
  case Col::Char:
    {
      const Chs* chs = col.m_chs;
      CHARSET_INFO* cs = chs->m_cs;
      unsigned n = 0;
      calcnokeychars(par, n, m_char);
      // extend by appropriate space
      (*cs->cset->fill)(cs, (char*)&m_char[n], col.m_bytelength - n, 0x20);
    }
    break;
  case Col::Varchar:
    {
      unsigned n = 0;
      calcnokeychars(par, n, m_varchar + 1);
      // set length and pad with nulls
      m_varchar[0] = n;
      memset(&m_varchar[1 + n], 0, col.m_bytelength - n);
    }
    break;
  case Col::Longvarchar:
    {
      unsigned n = 0;
      calcnokeychars(par, n, m_longvarchar + 2);
      // set length and pad with nulls
      m_longvarchar[0] = (n & 0xff);
      m_longvarchar[1] = (n >> 8);
      memset(&m_longvarchar[2 + n], 0, col.m_bytelength - n);
    }
    break;
  default:
    assert(false);
    break;
  }
}

void
Val::calcnokeychars(Par par, unsigned& n, unsigned char* buf)
{
  const Col& col = m_col;
  const Chs* chs = col.m_chs;
  CHARSET_INFO* cs = chs->m_cs;
  n = 0;
  unsigned len = 0;
  while (len < col.m_length) {
    if (urandom(1 + col.m_bytelength) == 0) {
      break;
    }
    unsigned half = maxcharcount / 2;
    int r = irandom((par.m_pctrange * half) / 100);
    if (par.m_bdir != 0 && urandom(10) != 0) {
      if (r < 0 && par.m_bdir > 0 || r > 0 && par.m_bdir < 0)
        r = -r;
    }
    unsigned i = half + r;
    assert(i < maxcharcount);
    const Chr& chr = chs->m_chr[i];
    assert(n + chr.m_size <= col.m_bytelength);
    memcpy(buf + n, chr.m_bytes, chr.m_size);
    n += chr.m_size;
    len++;
  }
}

int
Val::verify(Par par, const Val& val2) const
{
  CHK(cmp(par, val2) == 0);
  return 0;
}

int
Val::cmp(Par par, const Val& val2) const
{
  const Col& col = m_col;
  const Col& col2 = val2.m_col;
  assert(col.equal(col2));
  if (m_null || val2.m_null) {
    if (! m_null)
      return +1;
    if (! val2.m_null)
      return -1;
    return 0;
  }
  // verify data formats
  col.wellformed(dataaddr());
  col.wellformed(val2.dataaddr());
  // compare
  switch (col.m_type) {
  case Col::Unsigned:
    {
      if (m_uint32 < val2.m_uint32)
        return -1;
      if (m_uint32 > val2.m_uint32)
        return +1;
      return 0;
    }
    break;
  case Col::Char:
    {
      unsigned len = col.m_bytelength;
      return cmpchars(par, m_char, len, val2.m_char, len);
    }
    break;
  case Col::Varchar:
    {
      unsigned len1 = m_varchar[0];
      unsigned len2 = val2.m_varchar[0];
      return cmpchars(par, m_varchar + 1, len1, val2.m_varchar + 1, len2);
    }
    break;
  case Col::Longvarchar:
    {
      unsigned len1 = m_longvarchar[0] + (m_longvarchar[1] << 8);
      unsigned len2 = val2.m_longvarchar[0] + (val2.m_longvarchar[1] << 8);
      return cmpchars(par, m_longvarchar + 2, len1, val2.m_longvarchar + 2, len2);
    }
    break;
  default:
    break;
  }
  assert(false);
  return 0;
}

int
Val::cmpchars(Par par, const unsigned char* buf1, unsigned len1, const unsigned char* buf2, unsigned len2) const
{
  const Col& col = m_col;
  const Chs* chs = col.m_chs;
  CHARSET_INFO* cs = chs->m_cs;
  int k;
  if (! par.m_collsp) {
    unsigned char x1[maxxmulsize * 8000];
    unsigned char x2[maxxmulsize * 8000];
    // make strxfrm pad both to same length
    unsigned len = maxxmulsize * col.m_bytelength;
    int n1 = NdbSqlUtil::strnxfrm_bug7284(cs, x1, chs->m_xmul * len, buf1, len1);
    int n2 = NdbSqlUtil::strnxfrm_bug7284(cs, x2, chs->m_xmul * len, buf2, len2);
    assert(n1 != -1 && n1 == n2);
    k = memcmp(x1, x2, n1);
  } else {
    k = (*cs->coll->strnncollsp)(cs, buf1, len1, buf2, len2, false);
  }
  return k < 0 ? -1 : k > 0 ? +1 : 0;
}

static void
printstring(NdbOut& out, const unsigned char* str, unsigned len, bool showlen)
{
  char buf[4 * 8000];
  char *p = buf;
  *p++ = '[';
  if (showlen) {
    sprintf(p, "%u:", len);
    p += strlen(p);
  }
  for (unsigned i = 0; i < len; i++) {
    unsigned char c = str[i];
    if (c == '\\') {
      *p++ = '\\';
      *p++ = c;
    } else if (0x20 <= c && c < 0x7e) {
      *p++ = c;
    } else {
      *p++ = '\\';
      *p++ = hexstr[c >> 4];
      *p++ = hexstr[c & 15];
    }
  }
  *p++ = ']';
  *p = 0;
  out << buf;
}

static NdbOut&
operator<<(NdbOut& out, const Val& val)
{
  const Col& col = val.m_col;
  if (val.m_null) {
    out << "NULL";
    return out;
  }
  switch (col.m_type) {
  case Col::Unsigned:
    out << val.m_uint32;
    break;
  case Col::Char:
    {
      unsigned len = col.m_bytelength;
      printstring(out, val.m_char, len, false);
    }
    break;
  case Col::Varchar:
    {
      unsigned len = val.m_varchar[0];
      printstring(out, val.m_varchar + 1, len, true);
    }
    break;
  case Col::Longvarchar:
    {
      unsigned len = val.m_longvarchar[0] + (val.m_longvarchar[1] << 8);
      printstring(out, val.m_longvarchar + 2, len, true);
    }
    break;
  default:
    out << "type" << col.m_type;
    assert(false);
    break;
  }
  return out;
}

// Row - table tuple

struct Row {
  const Tab& m_tab;
  Val** m_val;
  bool m_exist;
  enum Op { NoOp = 0, ReadOp = 1, InsOp = 2, UpdOp = 4, DelOp = 8, AnyOp = 15 };
  Op m_pending;
  Row* m_dbrow;  // copy of db row before update
  Row(const Tab& tab);
  ~Row();
  void copy(const Row& row2);
  void calc(Par par, unsigned i, unsigned mask = 0);
  const Row& dbrow() const;
  int verify(Par par, const Row& row2) const;
  int insrow(Par par);
  int updrow(Par par);
  int updrow(Par par, const ITab& itab);
  int delrow(Par par);
  int delrow(Par par, const ITab& itab);
  int selrow(Par par);
  int selrow(Par par, const ITab& itab);
  int setrow(Par par);
  int cmp(Par par, const Row& row2) const;
  int cmp(Par par, const Row& row2, const ITab& itab) const;
private:
  Row& operator=(const Row& row2);
};

Row::Row(const Tab& tab) :
  m_tab(tab)
{
  m_val = new Val* [tab.m_cols];
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Col& col = *tab.m_col[k];
    m_val[k] = new Val(col);
  }
  m_exist = false;
  m_pending = NoOp;
  m_dbrow = 0;
}

Row::~Row()
{
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    delete m_val[k];
  }
  delete [] m_val;
  delete m_dbrow;
}

void
Row::copy(const Row& row2)
{
  const Tab& tab = m_tab;
  assert(&tab == &row2.m_tab);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    Val& val = *m_val[k];
    const Val& val2 = *row2.m_val[k];
    val.copy(val2);
  }
  m_exist = row2.m_exist;
  m_pending = row2.m_pending;
  if (row2.m_dbrow == 0) {
    m_dbrow = 0;
  } else {
    assert(row2.m_dbrow->m_dbrow == 0);
    if (m_dbrow == 0)
      m_dbrow = new Row(tab);
    m_dbrow->copy(*row2.m_dbrow);
  }
}

void
Row::calc(Par par, unsigned i, unsigned mask)
{
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    if (! (mask & (1 << k))) {
      Val& val = *m_val[k];
      val.calc(par, i);
    }
  }
}

const Row&
Row::dbrow() const
{
  if (m_dbrow == 0)
    return *this;
  assert(m_pending == Row::UpdOp || m_pending == Row::DelOp);
  return *m_dbrow;
}

int
Row::verify(Par par, const Row& row2) const
{
  const Tab& tab = m_tab;
  const Row& row1 = *this;
  assert(&row1.m_tab == &row2.m_tab && row1.m_exist && row2.m_exist);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val1 = *row1.m_val[k];
    const Val& val2 = *row2.m_val[k];
    CHK(val1.verify(par, val2) == 0);
  }
  return 0;
}

int
Row::insrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  assert(! m_exist);
  CHK(con.getNdbOperation(tab) == 0);
  CHKCON(con.m_op->insertTuple() == 0, con);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (col.m_pk)
      CHK(val.equal(par) == 0);
  }
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (! col.m_pk)
      CHK(val.setval(par) == 0);
  }
  m_pending = InsOp;
  return 0;
}

int
Row::updrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  assert(m_exist);
  CHK(con.getNdbOperation(tab) == 0);
  CHKCON(con.m_op->updateTuple() == 0, con);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (col.m_pk)
      CHK(val.equal(par) == 0);
  }
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (! col.m_pk)
      CHK(val.setval(par) == 0);
  }
  m_pending = UpdOp;
  return 0;
}

int
Row::updrow(Par par, const ITab& itab)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  assert(itab.m_type == ITab::UniqueHashIndex && &itab.m_tab == &tab);
  assert(m_exist);
  CHK(con.getNdbIndexOperation(itab, tab) == 0);
  CHKCON(con.m_op->updateTuple() == 0, con);
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const ICol& icol = *itab.m_icol[k];
    const Col& col = icol.m_col;
    unsigned m = col.m_num;
    const Val& val = *m_val[m];
    CHK(val.equal(par, icol) == 0);
  }
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (! col.m_pk)
      CHK(val.setval(par) == 0);
  }
  m_pending = UpdOp;
  return 0;
}

int
Row::delrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  assert(m_exist);
  CHK(con.getNdbOperation(m_tab) == 0);
  CHKCON(con.m_op->deleteTuple() == 0, con);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (col.m_pk)
      CHK(val.equal(par) == 0);
  }
  m_pending = DelOp;
  return 0;
}

int
Row::delrow(Par par, const ITab& itab)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  assert(itab.m_type == ITab::UniqueHashIndex && &itab.m_tab == &tab);
  assert(m_exist);
  CHK(con.getNdbIndexOperation(itab, tab) == 0);
  CHKCON(con.m_op->deleteTuple() == 0, con);
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const ICol& icol = *itab.m_icol[k];
    const Col& col = icol.m_col;
    unsigned m = col.m_num;
    const Val& val = *m_val[m];
    CHK(val.equal(par, icol) == 0);
  }
  m_pending = DelOp;
  return 0;
}

int
Row::selrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  CHK(con.getNdbOperation(m_tab) == 0);
  CHKCON(con.m_op->readTuple() == 0, con);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (col.m_pk)
      CHK(val.equal(par) == 0);
  }
  return 0;
}

int
Row::selrow(Par par, const ITab& itab)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  assert(itab.m_type == ITab::UniqueHashIndex && &itab.m_tab == &tab);
  CHK(con.getNdbIndexOperation(itab, tab) == 0);
  CHKCON(con.m_op->readTuple() == 0, con);
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const ICol& icol = *itab.m_icol[k];
    const Col& col = icol.m_col;
    unsigned m = col.m_num;
    const Val& val = *m_val[m];
    CHK(val.equal(par, icol) == 0);
  }
  return 0;
}

int
Row::setrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (! col.m_pk)
      CHK(val.setval(par) == 0);
  }
  m_pending = UpdOp;
  return 0;
}

int
Row::cmp(Par par, const Row& row2) const
{
  const Tab& tab = m_tab;
  assert(&tab == &row2.m_tab);
  int c = 0;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Val& val2 = *row2.m_val[k];
    if ((c = val.cmp(par, val2)) != 0)
      break;
  }
  return c;
}

int
Row::cmp(Par par, const Row& row2, const ITab& itab) const
{
  const Tab& tab = m_tab;
  int c = 0;
  for (unsigned i = 0; i < itab.m_icols; i++) {
    const ICol& icol = *itab.m_icol[i];
    const Col& col = icol.m_col;
    unsigned k = col.m_num;
    assert(k < tab.m_cols);
    const Val& val = *m_val[k];
    const Val& val2 = *row2.m_val[k];
    if ((c = val.cmp(par, val2)) != 0)
      break;
  }
  return c;
}

static NdbOut&
operator<<(NdbOut& out, const Row::Op op)
{
  if (op == Row::NoOp)
    out << "NoOp";
  else if (op == Row::InsOp)
    out << "InsOp";
  else if (op == Row::UpdOp)
    out << "UpdOp";
  else if (op == Row::DelOp)
    out << "DelOp";
  else
    out << op;
  return out;
}

static NdbOut&
operator<<(NdbOut& out, const Row& row)
{
  const Tab& tab = row.m_tab;
  for (unsigned i = 0; i < tab.m_cols; i++) {
    if (i > 0)
      out << " ";
    out << *row.m_val[i];
  }
  out << " exist=" << row.m_exist;
  if (row.m_pending)
    out << " pending=" << row.m_pending;
  if (row.m_dbrow != 0)
    out << " [dbrow=" << *row.m_dbrow << "]";
  return out;
}

static NdbOut&
operator<<(NdbOut& out, const Row* rowptr)
{
  if (rowptr == 0)
    out << "null";
  else
    out << *rowptr;
  return out;
}

// Set - set of table tuples

struct Set {
  const Tab& m_tab;
  unsigned m_rows;
  Row** m_row;
  unsigned* m_rowkey; // maps row number (from 0) in scan to tuple key
  Row* m_keyrow;
  NdbRecAttr** m_rec;
  Set(const Tab& tab, unsigned rows);
  ~Set();
  void reset();
  unsigned count() const;
  // old and new values
  bool exist(unsigned i) const;
  void dbsave(unsigned i);
  void calc(Par par, unsigned i, unsigned mask = 0);
  bool pending(unsigned i, unsigned mask) const;
  void notpending(unsigned i);
  void notpending(const Lst& lst);
  void dbdiscard(unsigned i);
  void dbdiscard(const Lst& lst);
  const Row& dbrow(unsigned i) const;
  // operations
  int insrow(Par par, unsigned i);
  int updrow(Par par, unsigned i);
  int updrow(Par par, const ITab& itab, unsigned i);
  int delrow(Par par, unsigned i);
  int delrow(Par par, const ITab& itab, unsigned i);
  int selrow(Par par, const Row& keyrow);
  int selrow(Par par, const ITab& itab, const Row& keyrow);
  // set and get
  void setkey(Par par, const Row& keyrow);
  void setkey(Par par, const ITab& itab, const Row& keyrow);
  int setrow(Par par, unsigned i);
  int getval(Par par);
  int getkey(Par par, unsigned* i);
  int putval(unsigned i, bool force, unsigned n = ~0);
  // verify
  int verify(Par par, const Set& set2) const;
  int verifyorder(Par par, const ITab& itab, bool descending) const;
  // protect structure
  NdbMutex* m_mutex;
  void lock() const {
    NdbMutex_Lock(m_mutex);
  }
  void unlock() const {
    NdbMutex_Unlock(m_mutex);
  }
private:
  Set& operator=(const Set& set2);
};

Set::Set(const Tab& tab, unsigned rows) :
  m_tab(tab)
{
  m_rows = rows;
  m_row = new Row* [m_rows];
  for (unsigned i = 0; i < m_rows; i++) {
    // allocate on need to save space
    m_row[i] = 0;
  }
  m_rowkey = new unsigned [m_rows];
  for (unsigned n = 0; n < m_rows; n++) {
    // initialize to null
    m_rowkey[n] = ~0;
  }
  m_keyrow = new Row(tab);
  m_rec = new NdbRecAttr* [tab.m_cols];
  for (unsigned k = 0; k < tab.m_cols; k++) {
    m_rec[k] = 0;
  }
  m_mutex = NdbMutex_Create();
  assert(m_mutex != 0);
}

Set::~Set()
{
  for (unsigned i = 0; i < m_rows; i++) {
    delete m_row[i];
  }
  delete [] m_row;
  delete [] m_rowkey;
  delete m_keyrow;
  delete [] m_rec;
  NdbMutex_Destroy(m_mutex);
}

void
Set::reset()
{
  for (unsigned i = 0; i < m_rows; i++) {
    if (m_row[i] != 0) {
      Row& row = *m_row[i];
      row.m_exist = false;
    }
  }
}

unsigned
Set::count() const
{
  unsigned count = 0;
  for (unsigned i = 0; i < m_rows; i++) {
    if (m_row[i] != 0) {
      Row& row = *m_row[i];
      if (row.m_exist)
        count++;
    }
  }
  return count;
}

// old and new values

bool
Set::exist(unsigned i) const
{
  assert(i < m_rows);
  if (m_row[i] == 0)    // not allocated => not exist
    return false;
  return m_row[i]->m_exist;
}

void
Set::dbsave(unsigned i)
{
  const Tab& tab = m_tab;
  assert(i < m_rows && m_row[i] != 0);
  Row& row = *m_row[i];
  LL5("dbsave " << i << ": " << row);
  assert(row.m_exist && ! row.m_pending && row.m_dbrow == 0);
  // could swap pointers but making copy is safer
  Row* rowptr = new Row(tab);
  rowptr->copy(row);
  row.m_dbrow = rowptr;
}

void
Set::calc(Par par, unsigned i, unsigned mask)
{
  const Tab& tab = m_tab;
  if (m_row[i] == 0)
    m_row[i] = new Row(tab);
  Row& row = *m_row[i];
  row.calc(par, i, mask);
}

bool
Set::pending(unsigned i, unsigned mask) const
{
  assert(i < m_rows);
  if (m_row[i] == 0)    // not allocated => not pending
    return Row::NoOp;
  return m_row[i]->m_pending & mask;
}

void
Set::notpending(unsigned i)
{
  assert(m_row[i] != 0);
  Row& row = *m_row[i];
  if (row.m_pending == Row::InsOp) {
    row.m_exist = true;
  } else if (row.m_pending == Row::UpdOp) {
    ;
  } else if (row.m_pending == Row::DelOp) {
    row.m_exist = false;
  }
  row.m_pending = Row::NoOp;
}

void
Set::notpending(const Lst& lst)
{
  for (unsigned j = 0; j < lst.m_cnt; j++) {
    unsigned i = lst.m_arr[j];
    notpending(i);
  }
}

void
Set::dbdiscard(unsigned i)
{
  assert(m_row[i] != 0);
  Row& row = *m_row[i];
  LL5("dbdiscard " << i << ": " << row);
  assert(row.m_dbrow != 0);
  delete row.m_dbrow;
  row.m_dbrow = 0;
}

const Row&
Set::dbrow(unsigned i) const
{
  assert(m_row[i] != 0);
  Row& row = *m_row[i];
  return row.dbrow();
}

void
Set::dbdiscard(const Lst& lst)
{
  for (unsigned j = 0; j < lst.m_cnt; j++) {
    unsigned i = lst.m_arr[j];
    dbdiscard(i);
  }
}

// operations

int
Set::insrow(Par par, unsigned i)
{
  assert(m_row[i] != 0);
  Row& row = *m_row[i];
  CHK(row.insrow(par) == 0);
  return 0;
}

int
Set::updrow(Par par, unsigned i)
{
  assert(m_row[i] != 0);
  Row& row = *m_row[i];
  CHK(row.updrow(par) == 0);
  return 0;
}

int
Set::updrow(Par par, const ITab& itab, unsigned i)
{
  assert(m_row[i] != 0);
  Row& row = *m_row[i];
  CHK(row.updrow(par, itab) == 0);
  return 0;
}

int
Set::delrow(Par par, unsigned i)
{
  assert(m_row[i] != 0);
  Row& row = *m_row[i];
  CHK(row.delrow(par) == 0);
  return 0;
}

int
Set::delrow(Par par, const ITab& itab, unsigned i)
{
  assert(m_row[i] != 0);
  Row& row = *m_row[i];
  CHK(row.delrow(par, itab) == 0);
  return 0;
}

int
Set::selrow(Par par, const Row& keyrow)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  setkey(par, keyrow);
  LL5("selrow " << tab.m_name << ": keyrow: " << keyrow);
  CHK(m_keyrow->selrow(par) == 0);
  CHK(getval(par) == 0);
  return 0;
}

int
Set::selrow(Par par, const ITab& itab, const Row& keyrow)
{
  Con& con = par.con();
  setkey(par, itab, keyrow);
  LL5("selrow " << itab.m_name << ": keyrow: " << keyrow);
  CHK(m_keyrow->selrow(par, itab) == 0);
  CHK(getval(par) == 0);
  return 0;
}

// set and get

void
Set::setkey(Par par, const Row& keyrow)
{
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Col& col = *tab.m_col[k];
    if (col.m_pk) {
      Val& val1 = *m_keyrow->m_val[k];
      const Val& val2 = *keyrow.dbrow().m_val[k];
      val1.copy(val2);
    }
  }
}

void
Set::setkey(Par par, const ITab& itab, const Row& keyrow)
{
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const ICol& icol = *itab.m_icol[k];
    const Col& col = icol.m_col;
    unsigned m = col.m_num;
    Val& val1 = *m_keyrow->m_val[m];
    const Val& val2 = *keyrow.dbrow().m_val[m];
    val1.copy(val2);
  }
}

int
Set::setrow(Par par, unsigned i)
{
  Con& con = par.con();
  assert(m_row[i] != 0);
  CHK(m_row[i]->setrow(par) == 0);
  return 0;
}

int
Set::getval(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    CHK(con.getValue(k, m_rec[k]) == 0);
  }
  return 0;
}

int
Set::getkey(Par par, unsigned* i)
{
  const Tab& tab = m_tab;
  unsigned k = tab.m_keycol;
  assert(m_rec[k] != 0);
  const char* aRef = m_rec[k]->aRef();
  Uint32 key = *(const Uint32*)aRef;
  CHK(key < m_rows);
  *i = key;
  return 0;
}

int
Set::putval(unsigned i, bool force, unsigned n)
{
  const Tab& tab = m_tab;
  if (m_row[i] == 0)
    m_row[i] = new Row(tab);
  Row& row = *m_row[i];
  CHK(! row.m_exist || force);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    Val& val = *row.m_val[k];
    NdbRecAttr* rec = m_rec[k];
    assert(rec != 0);
    if (rec->isNULL()) {
      val.m_null = true;
      continue;
    }
    const char* aRef = m_rec[k]->aRef();
    val.copy(aRef);
    val.m_null = false;
  }
  if (! row.m_exist)
    row.m_exist = true;
  if (n != ~0)
    m_rowkey[n] = i;
  return 0;
}

// verify

int
Set::verify(Par par, const Set& set2) const
{
  assert(&m_tab == &set2.m_tab && m_rows == set2.m_rows);
  LL4("verify set1 count=" << count() << " vs set2 count=" << set2.count());
  for (unsigned i = 0; i < m_rows; i++) {
    bool ok = true;
    if (exist(i) != set2.exist(i)) {
      ok = false;
    } else if (exist(i)) {
      if (dbrow(i).verify(par, set2.dbrow(i)) != 0)
        ok = false;
    }
    if (! ok) {
      LL1("verify failed: key=" << i << " row1=" << m_row[i] << " row2=" << set2.m_row[i]);
      CHK(0 == 1);
    }
  }
  return 0;
}

int
Set::verifyorder(Par par, const ITab& itab, bool descending) const
{
  const Tab& tab = m_tab;
  for (unsigned n = 0; n < m_rows; n++) {
    unsigned i2 = m_rowkey[n];
    if (i2 == ~0)
      break;
    if (n == 0)
      continue;
    unsigned i1 = m_rowkey[n - 1];
    assert(i1 < m_rows && i2 < m_rows);
    const Row& row1 = *m_row[i1];
    const Row& row2 = *m_row[i2];
    assert(row1.m_exist && row2.m_exist);
    if (! descending)
      CHK(row1.cmp(par, row2, itab) <= 0);
    else
      CHK(row1.cmp(par, row2, itab) >= 0);
  }
  return 0;
}

static NdbOut&
operator<<(NdbOut& out, const Set& set)
{
  for (unsigned i = 0; i < set.m_rows; i++) {
    const Row& row = *set.m_row[i];
    if (i > 0)
      out << endl;
    out << row;
  }
  return out;
}

// BVal - range scan bound

struct BVal : public Val {
  const ICol& m_icol;
  int m_type;
  BVal(const ICol& icol);
  int setbnd(Par par) const;
  int setflt(Par par) const;
};

BVal::BVal(const ICol& icol) :
  Val(icol.m_col),
  m_icol(icol)
{
}

int
BVal::setbnd(Par par) const
{
  Con& con = par.con();
  assert(g_compare_null || ! m_null);
  const char* addr = ! m_null ? (const char*)dataaddr() : 0;
  const ICol& icol = m_icol;
  CHK(con.setBound(icol.m_num, m_type, addr) == 0);
  return 0;
}

int
BVal::setflt(Par par) const
{
  static unsigned index_bound_to_filter_bound[5] = {
    NdbScanFilter::COND_GE,
    NdbScanFilter::COND_GT,
    NdbScanFilter::COND_LE,
    NdbScanFilter::COND_LT,
    NdbScanFilter::COND_EQ
  };
  Con& con = par.con();
  assert(g_compare_null || ! m_null);
  const char* addr = ! m_null ? (const char*)dataaddr() : 0;
  const ICol& icol = m_icol;
  const Col& col = icol.m_col;
  unsigned length = col.m_bytesize;
  unsigned cond = index_bound_to_filter_bound[m_type];
  CHK(con.setFilter(col.m_num, cond, addr, length) == 0);
  return 0;
}

static NdbOut&
operator<<(NdbOut& out, const BVal& bval)
{
  const ICol& icol = bval.m_icol;
  const Col& col = icol.m_col;
  const Val& val = bval;
  out << "type=" << bval.m_type;
  out << " icol=" << icol.m_num;
  out << " col=" << col.m_num << "," << col.m_name;
  out << " value=" << val;
  return out;
}

// BSet - set of bounds

struct BSet {
  const Tab& m_tab;
  const ITab& m_itab;
  unsigned m_alloc;
  unsigned m_bvals;
  BVal** m_bval;
  BSet(const Tab& tab, const ITab& itab, unsigned rows);
  ~BSet();
  void reset();
  void calc(Par par);
  void calcpk(Par par, unsigned i);
  int setbnd(Par par) const;
  int setflt(Par par) const;
  void filter(Par par, const Set& set, Set& set2) const;
};

BSet::BSet(const Tab& tab, const ITab& itab, unsigned rows) :
  m_tab(tab),
  m_itab(itab),
  m_alloc(2 * itab.m_icols),
  m_bvals(0)
{
  m_bval = new BVal* [m_alloc];
  for (unsigned i = 0; i < m_alloc; i++) {
    m_bval[i] = 0;
  }
}

BSet::~BSet()
{
  delete [] m_bval;
}

void
BSet::reset()
{
  while (m_bvals > 0) {
    unsigned i = --m_bvals;
    delete m_bval[i];
    m_bval[i] = 0;
  }
}

void
BSet::calc(Par par)
{
  const ITab& itab = m_itab;
  par.m_pctrange = par.m_pctbrange;
  reset();
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const ICol& icol = *itab.m_icol[k];
    const Col& col = icol.m_col;
    for (unsigned i = 0; i <= 1; i++) {
      if (m_bvals == 0 && urandom(100) == 0)
        return;
      if (m_bvals != 0 && urandom(3) == 0)
        return;
      assert(m_bvals < m_alloc);
      BVal& bval = *new BVal(icol);
      m_bval[m_bvals++] = &bval;
      bval.m_null = false;
      unsigned sel;
      do {
        // equality bound only on i==0
        sel = urandom(5 - i);
      } while (strchr(par.m_bound, '0' + sel) == 0);
      if (sel < 2)
        bval.m_type = 0 | (1 << i);
      else if (sel < 4)
        bval.m_type = 1 | (1 << i);
      else
        bval.m_type = 4;
      if (k + 1 < itab.m_icols)
        bval.m_type = 4;
      if (! g_compare_null)
        par.m_pctnull = 0;
      if (bval.m_type == 0 || bval.m_type == 1)
        par.m_bdir = -1;
      if (bval.m_type == 2 || bval.m_type == 3)
        par.m_bdir = +1;
      do {
        bval.calcnokey(par);
        if (i == 1) {
          assert(m_bvals >= 2);
          const BVal& bv1 = *m_bval[m_bvals - 2];
          const BVal& bv2 = *m_bval[m_bvals - 1];
          if (bv1.cmp(par, bv2) > 0 && urandom(100) != 0)
            continue;
        }
      } while (0);
      // equality bound only once
      if (bval.m_type == 4)
        break;
    }
  }
}

void
BSet::calcpk(Par par, unsigned i)
{
  const ITab& itab = m_itab;
  reset();
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const ICol& icol = *itab.m_icol[k];
    const Col& col = icol.m_col;
    assert(col.m_pk);
    assert(m_bvals < m_alloc);
    BVal& bval = *new BVal(icol);
    m_bval[m_bvals++] = &bval;
    bval.m_type = 4;
    bval.calc(par, i);
  }
}

int
BSet::setbnd(Par par) const
{
  if (m_bvals != 0) {
    unsigned p1 = urandom(m_bvals);
    unsigned p2 = 10009;        // prime
    // random order
    for (unsigned j = 0; j < m_bvals; j++) {
      unsigned k = p1 + p2 * j;
      const BVal& bval = *m_bval[k % m_bvals];
      CHK(bval.setbnd(par) == 0);
    }
    // duplicate
    if (urandom(5) == 0) {
      unsigned k = urandom(m_bvals);
      const BVal& bval = *m_bval[k];
      CHK(bval.setbnd(par) == 0);
    }
  }
  return 0;
}

int
BSet::setflt(Par par) const
{
  Con& con = par.con();
  CHK(con.getNdbScanFilter() == 0);
  CHK(con.beginFilter(NdbScanFilter::AND) == 0);
  if (m_bvals != 0) {
    unsigned p1 = urandom(m_bvals);
    unsigned p2 = 10009;        // prime
    const unsigned extras = 5;
    // random order
    for (unsigned j = 0; j < m_bvals + extras; j++) {
      unsigned k = p1 + p2 * j;
      const BVal& bval = *m_bval[k % m_bvals];
      CHK(bval.setflt(par) == 0);
    }
    // duplicate
    if (urandom(5) == 0) {
      unsigned k = urandom(m_bvals);
      const BVal& bval = *m_bval[k];
      CHK(bval.setflt(par) == 0);
    }
  }
  CHK(con.endFilter() == 0);
  return 0;
}

void
BSet::filter(Par par, const Set& set, Set& set2) const
{
  const Tab& tab = m_tab;
  const ITab& itab = m_itab;
  assert(&tab == &set2.m_tab && set.m_rows == set2.m_rows);
  assert(set2.count() == 0);
  for (unsigned i = 0; i < set.m_rows; i++) {
    if (! set.exist(i))
      continue;
    set.lock();
    const Row& row = set.dbrow(i);
    set.unlock();
    if (! g_store_null_key) {
      bool ok1 = false;
      for (unsigned k = 0; k < itab.m_icols; k++) {
        const ICol& icol = *itab.m_icol[k];
        const Col& col = icol.m_col;
        const Val& val = *row.m_val[col.m_num];
        if (! val.m_null) {
          ok1 = true;
          break;
        }
      }
      if (! ok1)
        continue;
    }
    bool ok2 = true;
    for (unsigned j = 0; j < m_bvals; j++) {
      const BVal& bval = *m_bval[j];
      const ICol& icol = bval.m_icol;
      const Col& col = icol.m_col;
      const Val& val = *row.m_val[col.m_num];
      int ret = bval.cmp(par, val);
      LL5("cmp: ret=" << ret << " " << bval << " vs " << val);
      if (bval.m_type == 0)
        ok2 = (ret <= 0);
      else if (bval.m_type == 1)
        ok2 = (ret < 0);
      else if (bval.m_type == 2)
        ok2 = (ret >= 0);
      else if (bval.m_type == 3)
        ok2 = (ret > 0);
      else if (bval.m_type == 4)
        ok2 = (ret == 0);
      else {
        assert(false);
      }
      if (! ok2)
        break;
    }
    if (! ok2)
      continue;
    if (set2.m_row[i] == 0)
      set2.m_row[i] = new Row(tab);
    Row& row2 = *set2.m_row[i];
    assert(! row2.m_exist);
    row2.copy(row);
  }
}

static NdbOut&
operator<<(NdbOut& out, const BSet& bset)
{
  out << "bounds=" << bset.m_bvals;
  for (unsigned j = 0; j < bset.m_bvals; j++) {
    const BVal& bval = *bset.m_bval[j];
    out << " [bound " << j << ": " << bval << "]";
  }
  return out;
}

// pk operations

static int
pkinsert(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("pkinsert " << tab.m_name);
  CHK(con.startTransaction() == 0);
  Lst lst;
  for (unsigned j = 0; j < par.m_rows; j++) {
    unsigned j2 = ! par.m_randomkey ? j : urandom(par.m_rows);
    unsigned i = thrrow(par, j2);
    set.lock();
    if (set.exist(i) || set.pending(i, Row::AnyOp)) {
      set.unlock();
      continue;
    }
    set.calc(par, i);
    CHK(set.insrow(par, i) == 0);
    set.unlock();
    LL4("pkinsert " << i << ": " << *set.m_row[i]);
    lst.push(i);
    if (lst.cnt() == par.m_batch) {
      bool deadlock = par.m_deadlock;
      CHK(con.execute(Commit, deadlock) == 0);
      con.closeTransaction();
      if (deadlock) {
        LL1("pkinsert: stop on deadlock [at 1]");
        return 0;
      }
      set.lock();
      set.notpending(lst);
      set.unlock();
      lst.reset();
      CHK(con.startTransaction() == 0);
    }
  }
  if (lst.cnt() != 0) {
    bool deadlock = par.m_deadlock;
    CHK(con.execute(Commit, deadlock) == 0);
    con.closeTransaction();
    if (deadlock) {
      LL1("pkinsert: stop on deadlock [at 2]");
      return 0;
    }
    set.lock();
    set.notpending(lst);
    set.unlock();
    return 0;
  }
  con.closeTransaction();
  return 0;
}

static int
pkupdate(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("pkupdate " << tab.m_name);
  CHK(con.startTransaction() == 0);
  Lst lst;
  bool deadlock = false;
  for (unsigned j = 0; j < par.m_rows; j++) {
    unsigned j2 = ! par.m_randomkey ? j : urandom(par.m_rows);
    unsigned i = thrrow(par, j2);
    set.lock();
    if (! set.exist(i) || set.pending(i, Row::AnyOp)) {
      set.unlock();
      continue;
    }
    set.dbsave(i);
    set.calc(par, i);
    CHK(set.updrow(par, i) == 0);
    set.unlock();
    LL4("pkupdate " << i << ": " << *set.m_row[i]);
    lst.push(i);
    if (lst.cnt() == par.m_batch) {
      deadlock = par.m_deadlock;
      CHK(con.execute(Commit, deadlock) == 0);
      if (deadlock) {
        LL1("pkupdate: stop on deadlock [at 1]");
        break;
      }
      con.closeTransaction();
      set.lock();
      set.notpending(lst);
      set.dbdiscard(lst);
      set.unlock();
      lst.reset();
      CHK(con.startTransaction() == 0);
    }
  }
  if (! deadlock && lst.cnt() != 0) {
    deadlock = par.m_deadlock;
    CHK(con.execute(Commit, deadlock) == 0);
    if (deadlock) {
      LL1("pkupdate: stop on deadlock [at 1]");
    } else {
      set.lock();
      set.notpending(lst);
      set.dbdiscard(lst);
      set.unlock();
    }
  }
  con.closeTransaction();
  return 0;
}

static int
pkdelete(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("pkdelete " << tab.m_name);
  CHK(con.startTransaction() == 0);
  Lst lst;
  bool deadlock = false;
  for (unsigned j = 0; j < par.m_rows; j++) {
    unsigned j2 = ! par.m_randomkey ? j : urandom(par.m_rows);
    unsigned i = thrrow(par, j2);
    set.lock();
    if (! set.exist(i) || set.pending(i, Row::AnyOp)) {
      set.unlock();
      continue;
    }
    CHK(set.delrow(par, i) == 0);
    set.unlock();
    LL4("pkdelete " << i << ": " << *set.m_row[i]);
    lst.push(i);
    if (lst.cnt() == par.m_batch) {
      deadlock = par.m_deadlock;
      CHK(con.execute(Commit, deadlock) == 0);
      if (deadlock) {
        LL1("pkdelete: stop on deadlock [at 1]");
        break;
      }
      con.closeTransaction();
      set.lock();
      set.notpending(lst);
      set.unlock();
      lst.reset();
      CHK(con.startTransaction() == 0);
    }
  }
  if (! deadlock && lst.cnt() != 0) {
    deadlock = par.m_deadlock;
    CHK(con.execute(Commit, deadlock) == 0);
    if (deadlock) {
      LL1("pkdelete: stop on deadlock [at 2]");
    } else {
      set.lock();
      set.notpending(lst);
      set.unlock();
    }
  }
  con.closeTransaction();
  return 0;
}

static int
pkread(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("pkread " << tab.m_name << " verify=" << par.m_verify);
  // expected
  const Set& set1 = set;
  Set set2(tab, set.m_rows);
  for (unsigned i = 0; i < set.m_rows; i++) {
    set.lock();
    if (! set.exist(i)) {
      set.unlock();
      continue;
    }
    set.unlock();
    CHK(con.startTransaction() == 0);
    CHK(set2.selrow(par, *set1.m_row[i]) == 0);
    CHK(con.execute(Commit) == 0);
    unsigned i2 = (unsigned)-1;
    CHK(set2.getkey(par, &i2) == 0 && i == i2);
    CHK(set2.putval(i, false) == 0);
    LL4("row " << set2.count() << ": " << *set2.m_row[i]);
    con.closeTransaction();
  }
  if (par.m_verify)
    CHK(set1.verify(par, set2) == 0);
  return 0;
}

static int
pkreadfast(Par par, unsigned count)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  const Set& set = par.set();
  LL3("pkfast " << tab.m_name);
  Row keyrow(tab);
  // not batched on purpose
  for (unsigned j = 0; j < count; j++) {
    unsigned i = urandom(set.m_rows);
    assert(set.exist(i));
    CHK(con.startTransaction() == 0);
    // define key
    keyrow.calc(par, i);
    CHK(keyrow.selrow(par) == 0);
    NdbRecAttr* rec;
    // get 1st column
    CHK(con.getValue((Uint32)0, rec) == 0);
    CHK(con.execute(Commit) == 0);
    con.closeTransaction();
  }
  return 0;
}

// hash index operations

static int
hashindexupdate(Par par, const ITab& itab)
{
  Con& con = par.con();
  Set& set = par.set();
  LL3("hashindexupdate " << itab.m_name);
  CHK(con.startTransaction() == 0);
  Lst lst;
  bool deadlock = false;
  for (unsigned j = 0; j < par.m_rows; j++) {
    unsigned j2 = ! par.m_randomkey ? j : urandom(par.m_rows);
    unsigned i = thrrow(par, j2);
    set.lock();
    if (! set.exist(i) || set.pending(i, Row::AnyOp)) {
      set.unlock();
      continue;
    }
    set.dbsave(i);
    // index key columns are not re-calculated
    set.calc(par, i, itab.m_colmask);
    CHK(set.updrow(par, itab, i) == 0);
    set.unlock();
    LL4("hashindexupdate " << i << ": " << *set.m_row[i]);
    lst.push(i);
    if (lst.cnt() == par.m_batch) {
      deadlock = par.m_deadlock;
      CHK(con.execute(Commit, deadlock) == 0);
      if (deadlock) {
        LL1("hashindexupdate: stop on deadlock [at 1]");
        break;
      }
      con.closeTransaction();
      set.lock();
      set.notpending(lst);
      set.dbdiscard(lst);
      set.unlock();
      lst.reset();
      CHK(con.startTransaction() == 0);
    }
  }
  if (! deadlock && lst.cnt() != 0) {
    deadlock = par.m_deadlock;
    CHK(con.execute(Commit, deadlock) == 0);
    if (deadlock) {
      LL1("hashindexupdate: stop on deadlock [at 1]");
    } else {
      set.lock();
      set.notpending(lst);
      set.dbdiscard(lst);
      set.unlock();
    }
  }
  con.closeTransaction();
  return 0;
}

static int
hashindexdelete(Par par, const ITab& itab)
{
  Con& con = par.con();
  Set& set = par.set();
  LL3("hashindexdelete " << itab.m_name);
  CHK(con.startTransaction() == 0);
  Lst lst;
  bool deadlock = false;
  for (unsigned j = 0; j < par.m_rows; j++) {
    unsigned j2 = ! par.m_randomkey ? j : urandom(par.m_rows);
    unsigned i = thrrow(par, j2);
    set.lock();
    if (! set.exist(i) || set.pending(i, Row::AnyOp)) {
      set.unlock();
      continue;
    }
    CHK(set.delrow(par, itab, i) == 0);
    set.unlock();
    LL4("hashindexdelete " << i << ": " << *set.m_row[i]);
    lst.push(i);
    if (lst.cnt() == par.m_batch) {
      deadlock = par.m_deadlock;
      CHK(con.execute(Commit, deadlock) == 0);
      if (deadlock) {
        LL1("hashindexdelete: stop on deadlock [at 1]");
        break;
      }
      con.closeTransaction();
      set.lock();
      set.notpending(lst);
      set.unlock();
      lst.reset();
      CHK(con.startTransaction() == 0);
    }
  }
  if (! deadlock && lst.cnt() != 0) {
    deadlock = par.m_deadlock;
    CHK(con.execute(Commit, deadlock) == 0);
    if (deadlock) {
      LL1("hashindexdelete: stop on deadlock [at 2]");
    } else {
      set.lock();
      set.notpending(lst);
      set.unlock();
    }
  }
  con.closeTransaction();
  return 0;
}

static int
hashindexread(Par par, const ITab& itab)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("hashindexread " << itab.m_name << " verify=" << par.m_verify);
  // expected
  const Set& set1 = set;
  Set set2(tab, set.m_rows);
  for (unsigned i = 0; i < set.m_rows; i++) {
    set.lock();
    if (! set.exist(i)) {
      set.unlock();
      continue;
    }
    set.unlock();
    CHK(con.startTransaction() == 0);
    CHK(set2.selrow(par, itab, *set1.m_row[i]) == 0);
    CHK(con.execute(Commit) == 0);
    unsigned i2 = (unsigned)-1;
    CHK(set2.getkey(par, &i2) == 0 && i == i2);
    CHK(set2.putval(i, false) == 0);
    LL4("row " << set2.count() << ": " << *set2.m_row[i]);
    con.closeTransaction();
  }
  if (par.m_verify)
    CHK(set1.verify(par, set2) == 0);
  return 0;
}

// scan read

static int
scanreadtable(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  const Set& set = par.set();
  // expected
  const Set& set1 = set;
  LL3("scanread " << tab.m_name << " lockmode=" << par.m_lockmode << " expect=" << set1.count() << " verify=" << par.m_verify);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.readTuples(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  unsigned n = 0;
  bool deadlock = false;
  while (1) {
    int ret;
    deadlock = par.m_deadlock;
    CHK((ret = con.nextScanResult(true, deadlock)) == 0 || ret == 1);
    if (ret == 1)
      break;
    if (deadlock) {
      LL1("scanreadtable: stop on deadlock");
      break;
    }
    unsigned i = (unsigned)-1;
    CHK(set2.getkey(par, &i) == 0);
    CHK(set2.putval(i, false, n) == 0);
    LL4("row " << n << ": " << *set2.m_row[i]);
    n++;
  }
  con.closeTransaction();
  if (par.m_verify)
    CHK(set1.verify(par, set2) == 0);
  LL3("scanread " << tab.m_name << " done rows=" << n);
  return 0;
}

static int
scanreadtablefast(Par par, unsigned countcheck)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  const Set& set = par.set();
  LL3("scanfast " << tab.m_name);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.readTuples(par) == 0);
  // get 1st column
  NdbRecAttr* rec;
  CHK(con.getValue((Uint32)0, rec) == 0);
  CHK(con.executeScan() == 0);
  unsigned count = 0;
  while (1) {
    int ret;
    CHK((ret = con.nextScanResult(true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    count++;
  }
  con.closeTransaction();
  CHK(count == countcheck);
  return 0;
}

static int
scanreadindex(Par par, const ITab& itab, BSet& bset, bool calc)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  const Set& set = par.set();
  Set set1(tab, set.m_rows);
  if (calc) {
    while (true) {
      bset.calc(par);
      bset.filter(par, set, set1);
      unsigned n = set1.count();
      // prefer proper subset
      if (0 < n && n < set.m_rows)
        break;
      if (urandom(3) == 0)
        break;
      set1.reset();
    }
  } else {
    bset.filter(par, set, set1);
  }
  LL3("scanread " << itab.m_name << " " << bset << " lockmode=" << par.m_lockmode << " expect=" << set1.count() << " verify=" << par.m_verify << " ordered=" << par.m_ordered << " descending=" << par.m_descending);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbIndexScanOperation(itab, tab) == 0);
  CHK(con.readIndexTuples(par) == 0);
  CHK(bset.setbnd(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  unsigned n = 0;
  bool deadlock = false;
  while (1) {
    int ret;
    deadlock = par.m_deadlock;
    CHK((ret = con.nextScanResult(true, deadlock)) == 0 || ret == 1);
    if (ret == 1)
      break;
    if (deadlock) {
      LL1("scanreadindex: stop on deadlock");
      break;
    }
    unsigned i = (unsigned)-1;
    CHK(set2.getkey(par, &i) == 0);
    CHK(set2.putval(i, par.m_dups, n) == 0);
    LL4("key " << i << " row " << n << ": " << *set2.m_row[i]);
    n++;
  }
  con.closeTransaction();
  if (par.m_verify) {
    CHK(set1.verify(par, set2) == 0);
    if (par.m_ordered)
      CHK(set2.verifyorder(par, itab, par.m_descending) == 0);
  }
  LL3("scanread " << itab.m_name << " done rows=" << n);
  return 0;
}

static int
scanreadindexfast(Par par, const ITab& itab, const BSet& bset, unsigned countcheck)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  const Set& set = par.set();
  LL3("scanfast " << itab.m_name << " " << bset);
  LL4(bset);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbIndexScanOperation(itab, tab) == 0);
  CHK(con.readIndexTuples(par) == 0);
  CHK(bset.setbnd(par) == 0);
  // get 1st column
  NdbRecAttr* rec;
  CHK(con.getValue((Uint32)0, rec) == 0);
  CHK(con.executeScan() == 0);
  unsigned count = 0;
  while (1) {
    int ret;
    CHK((ret = con.nextScanResult(true)) == 0 || ret == 1);
    if (ret == 1)
      break;
    count++;
  }
  con.closeTransaction();
  CHK(count == countcheck);
  return 0;
}

static int
scanreadfilter(Par par, const ITab& itab, BSet& bset, bool calc)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  const Set& set = par.set();
  Set set1(tab, set.m_rows);
  if (calc) {
    while (true) {
      bset.calc(par);
      bset.filter(par, set, set1);
      unsigned n = set1.count();
      // prefer proper subset
      if (0 < n && n < set.m_rows)
        break;
      if (urandom(3) == 0)
        break;
      set1.reset();
    }
  } else {
    bset.filter(par, set, set1);
  }
  LL3("scanfilter " << itab.m_name << " " << bset << " lockmode=" << par.m_lockmode << " expect=" << set1.count() << " verify=" << par.m_verify);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.readTuples(par) == 0);
  CHK(bset.setflt(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  unsigned n = 0;
  bool deadlock = false;
  while (1) {
    int ret;
    deadlock = par.m_deadlock;
    CHK((ret = con.nextScanResult(true, deadlock)) == 0 || ret == 1);
    if (ret == 1)
      break;
    if (deadlock) {
      LL1("scanfilter: stop on deadlock");
      break;
    }
    unsigned i = (unsigned)-1;
    CHK(set2.getkey(par, &i) == 0);
    CHK(set2.putval(i, par.m_dups, n) == 0);
    LL4("key " << i << " row " << n << ": " << *set2.m_row[i]);
    n++;
  }
  con.closeTransaction();
  if (par.m_verify) {
    CHK(set1.verify(par, set2) == 0);
  }
  LL3("scanfilter " << itab.m_name << " done rows=" << n);
  return 0;
}

static int
scanreadindex(Par par, const ITab& itab)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < par.m_subsubloop; i++) {
    if (itab.m_type == ITab::OrderedIndex) {
      BSet bset(tab, itab, par.m_rows);
      CHK(scanreadfilter(par, itab, bset, true) == 0);
      CHK(scanreadindex(par, itab, bset, true) == 0);
    }
  }
  return 0;
}

static int
scanreadindex(Par par)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0)
      continue;
    const ITab& itab = *tab.m_itab[i];
    if (itab.m_type == ITab::OrderedIndex) {
      CHK(scanreadindex(par, itab) == 0);
    } else {
      CHK(hashindexread(par, itab) == 0);
    }
  }
  return 0;
}

static int
scanreadall(Par par)
{
  CHK(scanreadtable(par) == 0);
  CHK(scanreadindex(par) == 0);
  return 0;
}

// timing scans

static int
timescantable(Par par)
{
  par.tmr().on();
  CHK(scanreadtablefast(par, par.m_totrows) == 0);
  par.tmr().off(par.set().m_rows);
  return 0;
}

static int
timescanpkindex(Par par)
{
  const Tab& tab = par.tab();
  const ITab& itab = *tab.m_itab[0];    // 1st index is on PK
  BSet bset(tab, itab, par.m_rows);
  par.tmr().on();
  CHK(scanreadindexfast(par, itab, bset, par.m_totrows) == 0);
  par.tmr().off(par.set().m_rows);
  return 0;
}

static int
timepkreadtable(Par par)
{
  par.tmr().on();
  unsigned count = par.m_samples;
  if (count == 0)
    count = par.m_totrows;
  CHK(pkreadfast(par, count) == 0);
  par.tmr().off(count);
  return 0;
}

static int
timepkreadindex(Par par)
{
  const Tab& tab = par.tab();
  const ITab& itab = *tab.m_itab[0];    // 1st index is on PK
  BSet bset(tab, itab, par.m_rows);
  unsigned count = par.m_samples;
  if (count == 0)
    count = par.m_totrows;
  par.tmr().on();
  for (unsigned j = 0; j < count; j++) {
    unsigned i = urandom(par.m_totrows);
    bset.calcpk(par, i);
    CHK(scanreadindexfast(par, itab, bset, 1) == 0);
  }
  par.tmr().off(count);
  return 0;
}

// scan update

static int
scanupdatetable(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("scan update " << tab.m_name);
  Set set2(tab, set.m_rows);
  par.m_lockmode = NdbOperation::LM_Exclusive;
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.readTuples(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  unsigned count = 0;
  // updating trans
  Con con2;
  con2.connect(con);
  CHK(con2.startTransaction() == 0);
  Lst lst;
  bool deadlock = false;
  while (1) {
    int ret;
    deadlock = par.m_deadlock;
    CHK((ret = con.nextScanResult(true, deadlock)) == 0 || ret == 1);
    if (ret == 1)
      break;
    if (deadlock) {
      LL1("scanupdatetable: stop on deadlock [at 1]");
      break;
    }
    if (par.m_scanstop != 0 && urandom(par.m_scanstop) == 0) {
      con.closeScan();
      break;
    }
    do {
      unsigned i = (unsigned)-1;
      CHK(set2.getkey(par, &i) == 0);
      const Row& row = *set.m_row[i];
      set.lock();
      if (! set.exist(i) || set.pending(i, Row::AnyOp)) {
        LL4("scan update " << tab.m_name << ": skip: " << row);
      } else {
        CHKTRY(set2.putval(i, false) == 0, set.unlock());
        CHKTRY(con.updateScanTuple(con2) == 0, set.unlock());
        Par par2 = par;
        par2.m_con = &con2;
        set.dbsave(i);
        set.calc(par, i);
        CHKTRY(set.setrow(par2, i) == 0, set.unlock());
        LL4("scan update " << tab.m_name << ": " << row);
        lst.push(i);
      }
      set.unlock();
      if (lst.cnt() == par.m_batch) {
        deadlock = par.m_deadlock;
        CHK(con2.execute(Commit, deadlock) == 0);
        if (deadlock) {
          LL1("scanupdatetable: stop on deadlock [at 2]");
          goto out;
        }
        con2.closeTransaction();
        set.lock();
        set.notpending(lst);
        set.dbdiscard(lst);
        set.unlock();
        count += lst.cnt();
        lst.reset();
        CHK(con2.startTransaction() == 0);
      }
      CHK((ret = con.nextScanResult(false)) == 0 || ret == 1 || ret == 2);
      if (ret == 2 && lst.cnt() != 0) {
        deadlock = par.m_deadlock;
        CHK(con2.execute(Commit, deadlock) == 0);
        if (deadlock) {
          LL1("scanupdatetable: stop on deadlock [at 3]");
          goto out;
        }
        con2.closeTransaction();
        set.lock();
        set.notpending(lst);
        set.dbdiscard(lst);
        set.unlock();
        count += lst.cnt();
        lst.reset();
        CHK(con2.startTransaction() == 0);
      }
    } while (ret == 0);
    if (ret == 1)
      break;
  }
out:
  con2.closeTransaction();
  LL3("scan update " << tab.m_name << " rows updated=" << count);
  con.closeTransaction();
  return 0;
}

static int
scanupdateindex(Par par, const ITab& itab, const BSet& bset)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("scan update " << itab.m_name);
  Set set2(tab, set.m_rows);
  par.m_lockmode = NdbOperation::LM_Exclusive;
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbIndexScanOperation(itab, tab) == 0);
  CHK(con.readTuples(par) == 0);
  CHK(bset.setbnd(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  unsigned count = 0;
  // updating trans
  Con con2;
  con2.connect(con);
  CHK(con2.startTransaction() == 0);
  Lst lst;
  bool deadlock = false;
  while (1) {
    int ret;
    deadlock = par.m_deadlock;
    CHK((ret = con.nextScanResult(true, deadlock)) == 0 || ret == 1);
    if (ret == 1)
      break;
    if (deadlock) {
      LL1("scanupdateindex: stop on deadlock [at 1]");
      break;
    }
    if (par.m_scanstop != 0 && urandom(par.m_scanstop) == 0) {
      con.closeScan();
      break;
    }
    do {
      unsigned i = (unsigned)-1;
      CHK(set2.getkey(par, &i) == 0);
      const Row& row = *set.m_row[i];
      set.lock();
      if (! set.exist(i) || set.pending(i, Row::AnyOp)) {
        LL4("scan update " << itab.m_name << ": skip: " << row);
      } else {
        CHKTRY(set2.putval(i, par.m_dups) == 0, set.unlock());
        CHKTRY(con.updateScanTuple(con2) == 0, set.unlock());
        Par par2 = par;
        par2.m_con = &con2;
        set.dbsave(i);
        set.calc(par, i);
        CHKTRY(set.setrow(par2, i) == 0, set.unlock());
        LL4("scan update " << itab.m_name << ": " << row);
        lst.push(i);
      }
      set.unlock();
      if (lst.cnt() == par.m_batch) {
        deadlock = par.m_deadlock;
        CHK(con2.execute(Commit, deadlock) == 0);
        if (deadlock) {
          LL1("scanupdateindex: stop on deadlock [at 2]");
          goto out;
        }
        con2.closeTransaction();
        set.lock();
        set.notpending(lst);
        set.dbdiscard(lst);
        set.unlock();
        count += lst.cnt();
        lst.reset();
        CHK(con2.startTransaction() == 0);
      }
      CHK((ret = con.nextScanResult(false)) == 0 || ret == 1 || ret == 2);
      if (ret == 2 && lst.cnt() != 0) {
        deadlock = par.m_deadlock;
        CHK(con2.execute(Commit, deadlock) == 0);
        if (deadlock) {
          LL1("scanupdateindex: stop on deadlock [at 3]");
          goto out;
        }
        con2.closeTransaction();
        set.lock();
        set.notpending(lst);
        set.dbdiscard(lst);
        set.unlock();
        count += lst.cnt();
        lst.reset();
        CHK(con2.startTransaction() == 0);
      }
    } while (ret == 0);
  }
out:
  con2.closeTransaction();
  LL3("scan update " << itab.m_name << " rows updated=" << count);
  con.closeTransaction();
  return 0;
}

static int
scanupdateindex(Par par, const ITab& itab)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < par.m_subsubloop; i++) {
    if (itab.m_type == ITab::OrderedIndex) {
      BSet bset(tab, itab, par.m_rows);
      bset.calc(par);
      CHK(scanupdateindex(par, itab, bset) == 0);
    } else {
      CHK(hashindexupdate(par, itab) == 0);
    }
  }
  return 0;
}

static int
scanupdateindex(Par par)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0)
      continue;
    const ITab& itab = *tab.m_itab[i];
    CHK(scanupdateindex(par, itab) == 0);
  }
  return 0;
}

static int
scanupdateall(Par par)
{
  CHK(scanupdatetable(par) == 0);
  CHK(scanupdateindex(par) == 0);
  return 0;
}

// medium level routines

static int
readverify(Par par)
{
  if (par.m_noverify)
    return 0;
  par.m_verify = true;
  par.m_lockmode = NdbOperation::LM_CommittedRead;
  CHK(pkread(par) == 0);
  CHK(scanreadall(par) == 0);
  return 0;
}

static int
readverifyfull(Par par)
{
  if (par.m_noverify)
    return 0;
  par.m_verify = true;
  par.m_lockmode = NdbOperation::LM_CommittedRead;
  const Tab& tab = par.tab();
  if (par.m_no == 0) {
    // thread 0 scans table
    CHK(scanreadtable(par) == 0);
  }
  // each thread scans different indexes
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (i % par.m_threads != par.m_no)
      continue;
    if (tab.m_itab[i] == 0)
      continue;
    const ITab& itab = *tab.m_itab[i];
    if (itab.m_type == ITab::OrderedIndex) {
      BSet bset(tab, itab, par.m_rows);
      CHK(scanreadindex(par, itab, bset, false) == 0);
    } else {
      CHK(hashindexread(par, itab) == 0);
    }
  }
  return 0;
}

static int
readverifyindex(Par par)
{
  if (par.m_noverify)
    return 0;
  par.m_verify = true;
  par.m_lockmode = NdbOperation::LM_CommittedRead;
  unsigned sel = urandom(10);
  if (sel < 9) {
    par.m_ordered = true;
    par.m_descending = (sel < 5);
  }
  CHK(scanreadindex(par) == 0);
  return 0;
}

static int
pkops(Par par)
{
  const Tab& tab = par.tab();
  par.m_randomkey = true;
  for (unsigned i = 0; i < par.m_subsubloop; i++) {
    unsigned j = 0;
    while (j < tab.m_itabs) {
      if (tab.m_itab[j] != 0) {
        const ITab& itab = *tab.m_itab[j];
        if (itab.m_type == ITab::UniqueHashIndex && urandom(5) == 0)
          break;
      }
      j++;
    }
    unsigned sel = urandom(10);
    if (par.m_slno % 2 == 0) {
      // favor insert
      if (sel < 8) {
        CHK(pkinsert(par) == 0);
      } else if (sel < 9) {
        if (j == tab.m_itabs)
          CHK(pkupdate(par) == 0);
        else {
          const ITab& itab = *tab.m_itab[j];
          CHK(hashindexupdate(par, itab) == 0);
        }
      } else {
        if (j == tab.m_itabs)
          CHK(pkdelete(par) == 0);
        else {
          const ITab& itab = *tab.m_itab[j];
          CHK(hashindexdelete(par, itab) == 0);
        }
      }
    } else {
      // favor delete
      if (sel < 1) {
        CHK(pkinsert(par) == 0);
      } else if (sel < 2) {
        if (j == tab.m_itabs)
          CHK(pkupdate(par) == 0);
        else {
          const ITab& itab = *tab.m_itab[j];
          CHK(hashindexupdate(par, itab) == 0);
        }
      } else {
        if (j == tab.m_itabs)
          CHK(pkdelete(par) == 0);
        else {
          const ITab& itab = *tab.m_itab[j];
          CHK(hashindexdelete(par, itab) == 0);
        }
      }
    }
  }
  return 0;
}

static int
pkupdatescanread(Par par)
{
  par.m_dups = true;
  par.m_deadlock = true;
  unsigned sel = urandom(10);
  if (sel < 5) {
    CHK(pkupdate(par) == 0);
  } else if (sel < 6) {
    par.m_verify = false;
    CHK(scanreadtable(par) == 0);
  } else {
    par.m_verify = false;
    if (sel < 8) {
      par.m_ordered = true;
      par.m_descending = (sel < 7);
    }
    CHK(scanreadindex(par) == 0);
  }
  return 0;
}

static int
mixedoperations(Par par)
{
  par.m_dups = true;
  par.m_deadlock = true;
  par.m_scanstop = par.m_totrows;       // randomly close scans
  unsigned sel = urandom(10);
  if (sel < 2) {
    CHK(pkdelete(par) == 0);
  } else if (sel < 4) {
    CHK(pkupdate(par) == 0);
  } else if (sel < 6) {
    CHK(scanupdatetable(par) == 0);
  } else {
    if (sel < 8) {
      par.m_ordered = true;
      par.m_descending = (sel < 7);
    }
    CHK(scanupdateindex(par) == 0);
  }
  return 0;
}

static int
pkupdateindexbuild(Par par)
{
  if (par.m_no == 0) {
    CHK(createindex(par) == 0);
  } else {
    par.m_randomkey = true;
    CHK(pkupdate(par) == 0);
  }
  return 0;
}

// threads

typedef int (*TFunc)(Par par);
enum TMode { ST = 1, MT = 2 };

extern "C" { static void* runthread(void* arg); }

struct Thr {
  enum State { Wait, Start, Stop, Stopped, Exit };
  State m_state;
  Par m_par;
  Uint64 m_id;
  NdbThread* m_thread;
  NdbMutex* m_mutex;
  NdbCondition* m_cond;
  TFunc m_func;
  int m_ret;
  void* m_status;
  Thr(Par par, unsigned n);
  ~Thr();
  int run();
  void start();
  void stop();
  void stopped();
  void exit();
  //
  void lock() {
    NdbMutex_Lock(m_mutex);
  }
  void unlock() {
    NdbMutex_Unlock(m_mutex);
  }
  void wait() {
    NdbCondition_Wait(m_cond, m_mutex);
  }
  void signal() {
    NdbCondition_Signal(m_cond);
  }
  void join() {
    NdbThread_WaitFor(m_thread, &m_status);
    m_thread = 0;
  }
};

Thr::Thr(Par par, unsigned n) :
  m_state(Wait),
  m_par(par),
  m_id(0),
  m_thread(0),
  m_mutex(0),
  m_cond(0),
  m_func(0),
  m_ret(0),
  m_status(0)
{
  m_par.m_no = n;
  char buf[10];
  sprintf(buf, "thr%03u", par.m_no);
  const char* name = strcpy(new char[10], buf);
  // mutex
  m_mutex = NdbMutex_Create();
  m_cond = NdbCondition_Create();
  assert(m_mutex != 0 && m_cond != 0);
  // run
  const unsigned stacksize = 256 * 1024;
  const NDB_THREAD_PRIO prio = NDB_THREAD_PRIO_LOW;
  m_thread = NdbThread_Create(runthread, (void**)this, stacksize, name, prio);
}

Thr::~Thr()
{
  if (m_thread != 0) {
    NdbThread_Destroy(&m_thread);
    m_thread = 0;
  }
  if (m_cond != 0) {
    NdbCondition_Destroy(m_cond);
    m_cond = 0;
  }
  if (m_mutex != 0) {
    NdbMutex_Destroy(m_mutex);
    m_mutex = 0;
  }
}

static void*
runthread(void* arg)
{
  Thr& thr = *(Thr*)arg;
  thr.m_id = (Uint64)pthread_self();
  if (thr.run() < 0) {
    LL1("exit on error");
  } else {
    LL4("exit ok");
  }
  return 0;
}

int
Thr::run()
{
  LL4("run");
  Con con;
  CHK(con.connect() == 0);
  m_par.m_con = &con;
  LL4("connected");
  while (1) {
    lock();
    while (m_state != Start && m_state != Exit) {
      LL4("wait");
      wait();
    }
    if (m_state == Exit) {
      LL4("exit");
      unlock();
      break;
    }
    LL4("start");
    assert(m_state == Start);
    m_ret = (*m_func)(m_par);
    m_state = Stopped;
    LL4("stop");
    signal();
    unlock();
    CHK(m_ret == 0);
  }
  con.disconnect();
  return 0;
}

void
Thr::start()
{
  lock();
  m_state = Start;
  signal();
  unlock();
}

void
Thr::stop()
{
  lock();
  m_state = Stop;
  signal();
  unlock();
}

void
Thr::stopped()
{
  lock();
  while (m_state != Stopped)
    wait();
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

// test run

static Thr** g_thrlist = 0;

static unsigned
getthrno()
{
  if (g_thrlist != 0) {
    Uint64 id = (Uint64)pthread_self();
    for (unsigned n = 0; n < g_opt.m_threads; n++) {
      if (g_thrlist[n] != 0) {
        const Thr& thr = *g_thrlist[n];
        if (thr.m_id == id)
          return thr.m_par.m_no;
      }
    }
  }
  return (unsigned)-1;
}

static int
runstep(Par par, const char* fname, TFunc func, unsigned mode)
{
  LL2(fname);
  const int threads = (mode & ST ? 1 : par.m_threads);
  int n; 
  for (n = 0; n < threads; n++) {
    LL4("start " << n);
    Thr& thr = *g_thrlist[n];
    thr.m_par.m_tab = par.m_tab;
    thr.m_par.m_set = par.m_set;
    thr.m_par.m_tmr = par.m_tmr;
    thr.m_par.m_lno = par.m_lno;
    thr.m_par.m_slno = par.m_slno;
    thr.m_func = func;
    thr.start();
  }
  unsigned errs = 0;
  for (n = threads - 1; n >= 0; n--) {
    LL4("stop " << n);
    Thr& thr = *g_thrlist[n];
    thr.stopped();
    if (thr.m_ret != 0)
      errs++;
  }
  CHK(errs == 0);
  return 0;
}

#define RUNSTEP(par, func, mode) CHK(runstep(par, #func, func, mode) == 0)

static int
tbuild(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    if (par.m_slno % 2 == 0) {
      RUNSTEP(par, createindex, ST);
      RUNSTEP(par, invalidateindex, MT);
      RUNSTEP(par, pkinsert, MT);
    } else {
      RUNSTEP(par, pkinsert, MT);
      RUNSTEP(par, createindex, ST);
      RUNSTEP(par, invalidateindex, MT);
    }
    RUNSTEP(par, pkupdate, MT);
    RUNSTEP(par, readverifyfull, MT);
    RUNSTEP(par, pkdelete, MT);
    RUNSTEP(par, readverifyfull, MT);
    RUNSTEP(par, dropindex, ST);
  }
  return 0;
}

static int
tindexscan(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, readverifyfull, MT);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    LL4("subloop " << par.m_slno);
    RUNSTEP(par, readverifyindex, MT);
  }
  return 0;
}


static int
tpkops(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    RUNSTEP(par, pkops, MT);
    LL2("rows=" << par.set().count());
    RUNSTEP(par, readverifyfull, MT);
  }
  return 0;
}

static int
tpkopsread(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, readverify, ST);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    RUNSTEP(par, pkupdatescanread, MT);
    RUNSTEP(par, readverify, ST);
  }
  RUNSTEP(par, pkdelete, MT);
  RUNSTEP(par, readverify, ST);
  return 0;
}

static int
tmixedops(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, readverify, ST);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    RUNSTEP(par, mixedoperations, MT);
    RUNSTEP(par, readverify, ST);
  }
  return 0;
}

static int
tbusybuild(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    RUNSTEP(par, pkupdateindexbuild, MT);
    RUNSTEP(par, invalidateindex, MT);
    RUNSTEP(par, readverify, ST);
    RUNSTEP(par, dropindex, ST);
  }
  return 0;
}

static int
ttimebuild(Par par)
{
  Tmr t1;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    RUNSTEP(par, pkinsert, MT);
    t1.on();
    RUNSTEP(par, createindex, ST);
    t1.off(par.m_totrows);
    RUNSTEP(par, invalidateindex, MT);
    RUNSTEP(par, dropindex, ST);
  }
  LL1("build index - " << t1.time());
  return 0;
}

static int
ttimemaint(Par par)
{
  Tmr t1, t2;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    RUNSTEP(par, pkinsert, MT);
    t1.on();
    RUNSTEP(par, pkupdate, MT);
    t1.off(par.m_totrows);
    RUNSTEP(par, createindex, ST);
    RUNSTEP(par, invalidateindex, MT);
    t2.on();
    RUNSTEP(par, pkupdate, MT);
    t2.off(par.m_totrows);
    RUNSTEP(par, dropindex, ST);
  }
  LL1("update - " << t1.time());
  LL1("update indexed - " << t2.time());
  LL1("overhead - " << t2.over(t1));
  return 0;
}

static int
ttimescan(Par par)
{
  if (par.tab().m_itab[0] == 0) {
    LL1("ttimescan - no index 0, skipped");
    return 0;
  }
  Tmr t1, t2;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    RUNSTEP(par, pkinsert, MT);
    RUNSTEP(par, createindex, ST);
    par.m_tmr = &t1;
    RUNSTEP(par, timescantable, ST);
    par.m_tmr = &t2;
    RUNSTEP(par, timescanpkindex, ST);
    RUNSTEP(par, dropindex, ST);
  }
  LL1("full scan table - " << t1.time());
  LL1("full scan PK index - " << t2.time());
  LL1("overhead - " << t2.over(t1));
  return 0;
}

static int
ttimepkread(Par par)
{
  if (par.tab().m_itab[0] == 0) {
    LL1("ttimescan - no index 0, skipped");
    return 0;
  }
  Tmr t1, t2;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_subloop; par.m_slno++) {
    RUNSTEP(par, pkinsert, MT);
    RUNSTEP(par, createindex, ST);
    par.m_tmr = &t1;
    RUNSTEP(par, timepkreadtable, ST);
    par.m_tmr = &t2;
    RUNSTEP(par, timepkreadindex, ST);
    RUNSTEP(par, dropindex, ST);
  }
  LL1("pk read table - " << t1.time());
  LL1("pk read PK index - " << t2.time());
  LL1("overhead - " << t2.over(t1));
  return 0;
}

static int
tdrop(Par par)
{
  RUNSTEP(par, droptable, ST);
  return 0;
}

struct TCase {
  const char* m_name;
  TFunc m_func;
  const char* m_desc;
  TCase(const char* name, TFunc func, const char* desc) :
    m_name(name),
    m_func(func),
    m_desc(desc) {
  }
};

static const TCase
tcaselist[] = {
  TCase("a", tbuild, "index build"),
  TCase("b", tindexscan, "index scans"),
  TCase("c", tpkops, "pk operations"),
  TCase("d", tpkopsread, "pk operations and scan reads"),
  TCase("e", tmixedops, "pk operations and scan operations"),
  TCase("f", tbusybuild, "pk operations and index build"),
  TCase("t", ttimebuild, "time index build"),
  TCase("u", ttimemaint, "time index maintenance"),
  TCase("v", ttimescan, "time full scan table vs index on pk"),
  TCase("w", ttimepkread, "time pk read table vs index on pk"),
  TCase("z", tdrop, "drop test tables")
};

static const unsigned
tcasecount = sizeof(tcaselist) / sizeof(tcaselist[0]);

static void
printcases()
{
  ndbout << "test cases:" << endl;
  for (unsigned i = 0; i < tcasecount; i++) {
    const TCase& tcase = tcaselist[i];
    ndbout << "  " << tcase.m_name << " - " << tcase.m_desc << endl;
  }
}

static void
printtables()
{
  Par par(g_opt);
  makebuiltintables(par);
  ndbout << "tables and indexes (x=ordered z=hash x0=on pk):" << endl;
  for (unsigned j = 0; j < tabcount; j++) {
    if (tablist[j] == 0)
      continue;
    const Tab& tab = *tablist[j];
    const char* tname = tab.m_name;
    ndbout << "  " << tname;
    for (unsigned i = 0; i < tab.m_itabs; i++) {
      if (tab.m_itab[i] == 0)
        continue;
      const ITab& itab = *tab.m_itab[i];
      const char* iname = itab.m_name;
      if (strncmp(tname, iname, strlen(tname)) == 0)
        iname += strlen(tname);
      ndbout << " " << iname;
      ndbout << "(";
      for (unsigned k = 0; k < itab.m_icols; k++) {
        if (k != 0)
          ndbout << ",";
        const ICol& icol = *itab.m_icol[k];
        const Col& col = icol.m_col;
        ndbout << col.m_name;
      }
      ndbout << ")";
    }
    ndbout << endl;
  }
}

static int
runtest(Par par)
{
  LL1("start");
  if (par.m_seed == -1) {
    // good enough for daily run
    unsigned short seed = (getpid() ^ time(0));
    LL1("random seed: " << seed);
    srandom((unsigned)seed);
  } else if (par.m_seed != 0) {
    LL1("random seed: " << par.m_seed);
    srandom(par.m_seed);
  } else {
    LL1("random seed: loop number");
  }
  // cs
  assert(par.m_csname != 0);
  if (strcmp(par.m_csname, "random") != 0) {
    CHARSET_INFO* cs;
    CHK((cs = get_charset_by_name(par.m_csname, MYF(0))) != 0 || (cs = get_charset_by_csname(par.m_csname, MY_CS_PRIMARY, MYF(0))) != 0);
    par.m_cs = cs;
  }
  // con
  Con con;
  CHK(con.connect() == 0);
  par.m_con = &con;
  // threads
  g_thrlist = new Thr* [par.m_threads];
  unsigned n;
  for (n = 0; n < par.m_threads; n++) {
    g_thrlist[n] = 0;
  }
  for (n = 0; n < par.m_threads; n++) {
    g_thrlist[n] = new Thr(par, n);
    Thr& thr = *g_thrlist[n];
    assert(thr.m_thread != 0);
  }
  for (par.m_lno = 0; par.m_loop == 0 || par.m_lno < par.m_loop; par.m_lno++) {
    LL1("loop " << par.m_lno);
    if (par.m_seed == 0)
      srandom(par.m_lno);
    for (unsigned i = 0; i < tcasecount; i++) {
      const TCase& tcase = tcaselist[i];
      if (par.m_case != 0 && strchr(par.m_case, tcase.m_name[0]) == 0)
        continue;
      makebuiltintables(par);
      LL1("case " << tcase.m_name << " - " << tcase.m_desc);
      for (unsigned j = 0; j < tabcount; j++) {
        if (tablist[j] == 0)
          continue;
        const Tab& tab = *tablist[j];
        par.m_tab = &tab;
        par.m_set = new Set(tab, par.m_totrows);
        LL1("table " << tab.m_name);
        CHK(tcase.m_func(par) == 0);
        delete par.m_set;
        par.m_set = 0;
      }
    }
  }
  for (n = 0; n < par.m_threads; n++) {
    Thr& thr = *g_thrlist[n];
    thr.exit();
  }
  for (n = 0; n < par.m_threads; n++) {
    Thr& thr = *g_thrlist[n];
    thr.join();
    delete &thr;
  }
  delete [] g_thrlist;
  g_thrlist = 0;
  con.disconnect();
  LL1("done");
  return 0;
}

NDB_COMMAND(testOIBasic, "testOIBasic", "testOIBasic", "testOIBasic", 65535)
{
  ndb_init();
  if (ndbout_mutex == NULL)
    ndbout_mutex = NdbMutex_Create();
  while (++argv, --argc > 0) {
    const char* arg = argv[0];
    if (*arg != '-') {
      ndbout << "testOIBasic: unknown argument " << arg;
      goto usage;
    }
    if (strcmp(arg, "-batch") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_batch = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-bound") == 0) {
      if (++argv, --argc > 0) {
        const char* p = argv[0];
        if (strlen(p) != 0 && strlen(p) == strspn(p, "01234")) {
          g_opt.m_bound = strdup(p);
          continue;
        }
      }
    }
    if (strcmp(arg, "-case") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_case = strdup(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-collsp") == 0) {
      g_opt.m_collsp = true;
      continue;
    }
    if (strcmp(arg, "-core") == 0) {
      g_opt.m_core = true;
      continue;
    }
    if (strcmp(arg, "-csname") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_csname = strdup(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-die") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_die = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-dups") == 0) {
      g_opt.m_dups = true;
      continue;
    }
    if (strcmp(arg, "-fragtype") == 0) {
      if (++argv, --argc > 0) {
        if (strcmp(argv[0], "single") == 0) {
          g_opt.m_fragtype = NdbDictionary::Object::FragSingle;
          continue;
        }
        if (strcmp(argv[0], "small") == 0) {
          g_opt.m_fragtype = NdbDictionary::Object::FragAllSmall;
          continue;
        }
        if (strcmp(argv[0], "medium") == 0) {
          g_opt.m_fragtype = NdbDictionary::Object::FragAllMedium;
          continue;
        }
        if (strcmp(argv[0], "large") == 0) {
          g_opt.m_fragtype = NdbDictionary::Object::FragAllLarge;
          continue;
        }
      }
    }
    if (strcmp(arg, "-index") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_index = strdup(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-loop") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_loop = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-nologging") == 0) {
      g_opt.m_nologging = true;
      continue;
    }
    if (strcmp(arg, "-noverify") == 0) {
      g_opt.m_noverify = true;
      continue;
    }
    if (strcmp(arg, "-pctnull") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_pctnull = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-rows") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_rows = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-samples") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_samples = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-scanpar") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_scanpar = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-seed") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_seed = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-subloop") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_subloop = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-table") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_table = strdup(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-threads") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_threads = atoi(argv[0]);
        if (1 <= g_opt.m_threads)
          continue;
      }
    }
    if (strcmp(arg, "-v") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_v = atoi(argv[0]);
        continue;
      }
    }
    if (strncmp(arg, "-v", 2) == 0 && isdigit(arg[2])) {
      g_opt.m_v = atoi(&arg[2]);
      continue;
    }
    if (strcmp(arg, "-h") == 0 || strcmp(arg, "-help") == 0) {
      printhelp();
      goto wrongargs;
    }
    ndbout << "testOIBasic: bad or unknown option " << arg;
    goto usage;
  }
  {
    Par par(g_opt);
    g_ncc = new Ndb_cluster_connection();
    if (g_ncc->connect(30) != 0 || runtest(par) < 0)
      goto failed;
    delete g_ncc;
    g_ncc = 0;
  }
ok:
  return NDBT_ProgramExit(NDBT_OK);
failed:
  return NDBT_ProgramExit(NDBT_FAILED);
usage:
  ndbout << " (use -h for help)" << endl;
wrongargs:
  return NDBT_ProgramExit(NDBT_WRONGARGS);
}

// vim: set sw=2 et:
