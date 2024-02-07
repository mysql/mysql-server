/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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
#include "util/require.h"

#include <NdbCondition.h>
#include <NdbHost.h>
#include <NdbMutex.h>
#include <NdbSleep.h>
#include <NdbThread.h>
#include <NdbTick.h>
#include <ndb_version.h>
#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbTest.hpp>
#include "my_sys.h"
#include "mysql/strings/m_ctype.h"

// options

struct Opt {
  // common options
  uint m_batch;
  const char *m_bound;
  const char *m_case;
  bool m_cont;
  bool m_core;
  const char *m_csname;
  CHARSET_INFO *m_cs;
  int m_die;
  bool m_dups;
  NdbDictionary::Object::FragmentType m_fragtype;
  const char *m_index;
  uint m_loop;
  uint m_mrrmaxrng;
  bool m_msglock;
  bool m_nologging;
  bool m_noverify;
  uint m_pctmrr;
  uint m_pctnull;
  uint m_rows;
  uint m_samples;
  uint m_scanbatch;
  uint m_scanpar;
  uint m_scanstop;
  int m_seed;
  const char *m_skip;
  uint m_sloop;
  uint m_ssloop;
  const char *m_table;
  uint m_threads;
  int m_v;  // int for lint
  Opt()
      : m_batch(32),
        m_bound("01234"),
        m_case(0),
        m_cont(false),
        m_core(false),
        m_csname("random"),
        m_cs(0),
        m_die(0),
        m_dups(false),
        m_fragtype(NdbDictionary::Object::FragUndefined),
        m_index(0),
        m_loop(1),
        m_mrrmaxrng(10),
        m_msglock(true),
        m_nologging(false),
        m_noverify(false),
        m_pctmrr(50),
        m_pctnull(10),
        m_rows(1000),
        m_samples(0),
        m_scanbatch(0),
        m_scanpar(0),
        m_scanstop(0),
        m_seed(-1),
        m_skip(0),
        m_sloop(4),
        m_ssloop(4),
        m_table(0),
        m_threads(4),
        m_v(1) {}
};

static Opt g_opt;

static void printcases();
static void printtables();

static void printhelp() {
  Opt d;
  ndbout << "usage: testOIbasic [options]" << endl
         << "  -batch N      pk operations in batch [" << d.m_batch << "]"
         << endl
         << "  -bound xyz    use only these bound types 0-4 [" << d.m_bound
         << "]" << endl
         << "  -case abc     only given test cases (letters a-z)" << endl
         << "  -cont         on error continue to next test case [" << d.m_cont
         << "]" << endl
         << "  -core         core dump on error [" << d.m_core << "]" << endl
         << "  -csname S     charset or collation [" << d.m_csname << "]"
         << endl
         << "  -die nnn      exit immediately on NDB error code nnn" << endl
         << "  -dups         allow duplicate tuples from index scan ["
         << d.m_dups << "]" << endl
         << "  -fragtype T   fragment type single/small/medium/large" << endl
         << "  -index xyz    only given index numbers (digits 0-9)" << endl
         << "  -loop N       loop count full suite 0=forever [" << d.m_loop
         << "]" << endl
         << "  -mrrmaxrng N  max ranges to supply for MRR scan ["
         << d.m_mrrmaxrng << "]" << endl
         << "  -nologging    create tables in no-logging mode" << endl
         << "  -noverify     skip index verifications" << endl
         << "  -pctmrr N     pct of index scans to use MRR [" << d.m_pctmrr
         << "]" << endl
         << "  -pctnull N    pct NULL values in nullable column ["
         << d.m_pctnull << "]" << endl
         << "  -rows N       rows per thread [" << d.m_rows << "]" << endl
         << "  -samples N    samples for some timings (0=all) [" << d.m_samples
         << "]" << endl
         << "  -scanbatch N  scan batch 0=default [" << d.m_scanbatch << "]"
         << endl
         << "  -scanpar N    scan parallel 0=default [" << d.m_scanpar << "]"
         << endl
         << "  -seed N       srandom seed 0=loop number -1=random [" << d.m_seed
         << "]" << endl
         << "  -skip abc     skip given test cases (letters a-z)" << endl
         << "  -sloop N      level 2 (sub)loop count [" << d.m_sloop << "]"
         << endl
         << "  -ssloop N     level 3 (sub)loop count [" << d.m_ssloop << "]"
         << endl
         << "  -table xyz    only given table numbers (digits 0-9)" << endl
         << "  -threads N    number of threads [" << d.m_threads << "]" << endl
         << "  -vN           verbosity [" << d.m_v << "]" << endl
         << "  -h or -help   print this help text" << endl;
  printcases();
  printtables();
}

// not yet configurable
static const bool g_store_null_key = true;

// compare NULL like normal value (NULL < not NULL, NULL == NULL)
static const bool g_compare_null = true;

static const char *hexstr = "0123456789abcdef";

// random ints
#ifdef _WIN32
#define random() rand()
#define srandom(SEED) srand(SEED)
#endif

static uint urandom(uint n) {
  if (n == 0) return 0;
  uint i = random() % n;
  return i;
}

static int irandom(uint n) {
  if (n == 0) return 0;
  int i = random() % n;
  if (random() & 0x1) i = -i;
  return i;
}

static bool randompct(uint pct) {
  if (pct == 0) return false;
  if (pct >= 100) return true;
  return urandom(100) < pct;
}

static uint random_coprime(uint n) {
  uint prime[] = {101, 211, 307, 401, 503, 601, 701, 809, 907};
  uint count = sizeof(prime) / sizeof(prime[0]);
  if (n == 0) return 0;
  while (1) {
    uint i = urandom(count);
    if (n % prime[i] != 0) return prime[i];
  }
}

// random re-sequence of 0...(n-1)

struct Rsq {
  Rsq(uint n);
  uint next();

 private:
  uint m_n;
  uint m_i;
  uint m_start;
  uint m_prime;
};

Rsq::Rsq(uint n) {
  m_n = n;
  m_i = 0;
  m_start = urandom(n);
  m_prime = random_coprime(n);
}

uint Rsq::next() {
  require(m_n != 0);
  return (m_start + m_i++ * m_prime) % m_n;
}

// log and error macros

static NdbMutex *ndbout_mutex = NULL;
static const char *getthrprefix();

#define LLN(n, s)                                       \
  do {                                                  \
    if ((n) > g_opt.m_v) break;                         \
    if (g_opt.m_msglock) NdbMutex_Lock(ndbout_mutex);   \
    ndbout << getthrprefix();                           \
    if ((n) > 2) ndbout << "line " << __LINE__ << ": "; \
    ndbout << s << endl;                                \
    if (g_opt.m_msglock) NdbMutex_Unlock(ndbout_mutex); \
  } while (0)

#define LL0(s) LLN(0, s)
#define LL1(s) LLN(1, s)
#define LL2(s) LLN(2, s)
#define LL3(s) LLN(3, s)
#define LL4(s) LLN(4, s)
#define LL5(s) LLN(5, s)

#define HEX(x) hex << (x) << dec

// following check a condition and return -1 on failure

#undef CHK     // simple check
#undef CHKTRY  // check with action on fail
#undef CHKCON  // print NDB API errors on failure

#define CHK(x) CHKTRY(x, ;)

#define CHKTRY(x, act)                                   \
  do {                                                   \
    if (x) break;                                        \
    LL0("line " << __LINE__ << ": " << #x << " failed"); \
    if (g_opt.m_core) abort();                           \
    act;                                                 \
    return -1;                                           \
  } while (0)

#define CHKCON(x, con)                                   \
  do {                                                   \
    if (x) break;                                        \
    LL0("line " << __LINE__ << ": " << #x << " failed"); \
    (con).printerror(ndbout);                            \
    if (g_opt.m_core) abort();                           \
    return -1;                                           \
  } while (0)

// method parameters

struct Thr;
struct Con;
struct Tab;
struct ITab;
struct Set;
struct Tmr;

struct Par : public Opt {
  uint m_no;
  Con *m_con;
  Con &con() const {
    require(m_con != 0);
    return *m_con;
  }
  const Tab *m_tab;
  const Tab &tab() const {
    require(m_tab != 0);
    return *m_tab;
  }
  const ITab *m_itab;
  const ITab &itab() const {
    require(m_itab != 0);
    return *m_itab;
  }
  Set *m_set;
  Set &set() const {
    require(m_set != 0);
    return *m_set;
  }
  Tmr *m_tmr;
  Tmr &tmr() const {
    require(m_tmr != 0);
    return *m_tmr;
  }
  char m_currcase[2];
  uint m_lno;
  uint m_slno;
  uint m_totrows;
  // value calculation
  uint m_range;
  uint m_pctrange;
  uint m_pctbrange;
  int m_bdir;
  bool m_noindexkeyupdate;
  // choice of key
  bool m_randomkey;
  // do verify after read
  bool m_verify;
  // errors to catch (see Con)
  uint m_catcherr;
  // abort percentage
  uint m_abortpct;
  NdbOperation::LockMode m_lockmode;
  // scan options
  bool m_tupscan;
  bool m_ordered;
  bool m_descending;
  bool m_multiRange;
  // threads used by current test case
  uint m_usedthreads;
  Par(const Opt &opt)
      : Opt(opt),
        m_no(0),
        m_con(0),
        m_tab(0),
        m_itab(0),
        m_set(0),
        m_tmr(0),
        m_lno(0),
        m_slno(0),
        m_totrows(0),
        m_range(m_rows),
        m_pctrange(40),
        m_pctbrange(80),
        m_bdir(0),
        m_noindexkeyupdate(false),
        m_randomkey(false),
        m_verify(false),
        m_catcherr(0),
        m_abortpct(0),
        m_lockmode(NdbOperation::LM_Read),
        m_tupscan(false),
        m_ordered(false),
        m_descending(false),
        m_multiRange(false),
        m_usedthreads(0) {
    m_currcase[0] = 0;
  }
};

static bool usetable(const Par &par, uint i) {
  return par.m_table == 0 || strchr(par.m_table, '0' + i) != 0;
}

static bool useindex(const Par &par, uint i) {
  return par.m_index == 0 || strchr(par.m_index, '0' + i) != 0;
}

static uint thrrow(const Par &par, uint j) {
  return par.m_usedthreads * j + par.m_no;
}

#if 0
static bool
isthrrow(const Par& par, uint i)
{
  return i % par.m_usedthreads == par.m_no;
}
#endif

// timer

struct Tmr {
  void clr();
  void on();
  void off(uint cnt = 0);
  const char *time();
  const char *pct(const Tmr &t1);
  const char *over(const Tmr &t1);
  Uint64 m_on;
  Uint64 m_ms;
  uint m_cnt;
  char m_time[100];
  char m_text[100];
  Tmr() { clr(); }
};

void Tmr::clr() { m_on = m_ms = m_cnt = m_time[0] = m_text[0] = 0; }

void Tmr::on() {
  require(m_on == 0);
  m_on = NdbTick_CurrentMillisecond();
}

void Tmr::off(uint cnt) {
  const Uint64 off = NdbTick_CurrentMillisecond();
  require(m_on != 0 && off >= m_on);
  m_ms += off - m_on;
  m_cnt += cnt;
  m_on = 0;
}

const char *Tmr::time() {
  if (m_cnt == 0) {
    sprintf(m_time, "%u ms", (unsigned)m_ms);
  } else {
    sprintf(m_time, "%u ms per %u ( %u ms per 1000 )", (unsigned)m_ms, m_cnt,
            (unsigned)((1000 * m_ms) / m_cnt));
  }
  return m_time;
}

const char *Tmr::pct(const Tmr &t1) {
  if (0 < t1.m_ms) {
    sprintf(m_text, "%u pct", (unsigned)((100 * m_ms) / t1.m_ms));
  } else {
    sprintf(m_text, "[cannot measure]");
  }
  return m_text;
}

const char *Tmr::over(const Tmr &t1) {
  if (0 < t1.m_ms) {
    if (t1.m_ms <= m_ms)
      sprintf(m_text, "%u pct", (unsigned)((100 * (m_ms - t1.m_ms)) / t1.m_ms));
    else
      sprintf(m_text, "-%u pct",
              (unsigned)((100 * (t1.m_ms - m_ms)) / t1.m_ms));
  } else {
    sprintf(m_text, "[cannot measure]");
  }
  return m_text;
}

// character sets

static const uint maxcsnumber = 512;
static const uint maxcharcount = 32;
static const uint maxcharsize = 4;

// single mb char
struct Chr {
  uchar m_bytes[maxcharsize];
  uint m_size;  // Actual size of m_bytes[]
  Chr();
};

Chr::Chr() {
  memset(m_bytes, 0, sizeof(m_bytes));
  m_size = 0;
}

// charset and random valid chars to use
struct Chs {
  CHARSET_INFO *m_cs;
  Chr *m_chr;
  Chs(CHARSET_INFO *cs);
  ~Chs();
};

static NdbOut &operator<<(NdbOut &out, const Chs &chs);

Chs::Chs(CHARSET_INFO *cs) : m_cs(cs) {
  require(m_cs->mbmaxlen <= maxcharsize);

  m_chr = new Chr[maxcharcount];
  uint i = 0;
  uint miss1 = 0;
  uint miss4 = 0;
  while (i < maxcharcount) {
    uchar *bytes = m_chr[i].m_bytes;
    uint size = 0;
    bool ok = false;
    do {
      bytes[size++] = urandom(256);

      int not_used;
      const char *sbytes = (const char *)bytes;
      if ((*cs->cset->well_formed_len)(cs, sbytes, sbytes + size, size,
                                       &not_used) == size) {
        // Break when a well_formed Chr has been produced.
        ok = true;
        break;
      }
    } while (size < m_cs->mbmaxlen);

    if (!ok) {  // Chr never became well_formed.
      miss1++;
      continue;
    }

    // check for duplicate
    for (uint j = 0; j < i; j++) {
      const Chr &chr = m_chr[j];
      if ((*cs->coll->strnncollsp)(cs, chr.m_bytes, chr.m_size, bytes, size) ==
          0) {
        ok = false;
        break;
      }
    }
    if (!ok) {
      miss4++;
      continue;
    }
    m_chr[i].m_size = size;
    i++;
  }
  bool disorder = true;
  uint bubbles = 0;
  while (disorder) {
    disorder = false;
    for (uint i = 1; i < maxcharcount; i++) {
      if ((*cs->coll->strnncollsp)(cs, m_chr[i - 1].m_bytes,
                                   m_chr[i - 1].m_size, m_chr[i].m_bytes,
                                   m_chr[i].m_size) > 0) {
        const Chr chr = m_chr[i];
        m_chr[i] = m_chr[i - 1];
        m_chr[i - 1] = chr;
        disorder = true;
        bubbles++;
      }
    }
  }
  LL3("inited charset " << *this << " miss=" << miss1 << "," << miss4
                        << " bubbles=" << bubbles);
}

Chs::~Chs() { delete[] m_chr; }

static NdbOut &operator<<(NdbOut &out, const Chs &chs) {
  CHARSET_INFO *cs = chs.m_cs;
  out << cs->m_coll_name << "[" << cs->mbminlen << "-" << cs->mbmaxlen << "]";
  return out;
}

static Chs *cslist[maxcsnumber];

static void initcslist() {
  for (uint i = 0; i < maxcsnumber; i++) {
    cslist[i] = 0;
  }
}

static void resetcslist() {
  for (uint i = 0; i < maxcsnumber; i++) {
    delete cslist[i];
    cslist[i] = 0;
  }
}

static Chs *getcs(const Par &par) {
  CHARSET_INFO *cs;
  if (par.m_cs != 0) {
    cs = par.m_cs;
  } else {
    while (1) {
      uint n = urandom(maxcsnumber);
      cs = get_charset(n, MYF(0));
      if (cs != 0) {
        // avoid dodgy internal character sets
        // see bug# 37554
        if (cs->state & MY_CS_HIDDEN) continue;

        // the utf32_ charsets does for unknown "not work"
        // not work == endless loop in Chs::Chs
        // by default these are not compiled in 7.0...
        // but in 7.2 they are...so testOIbasic always fails in 7.2
        if (strncmp(cs->m_coll_name, "utf32_", sizeof("utf32_") - 1) == 0)
          continue;

        // prefer complex charsets
        if (cs->mbmaxlen != 1 || urandom(5) == 0) break;
      }
    }
  }
  ndbout << "Use charset: " << cs->m_coll_name << endl;
  if (cslist[cs->number] == 0) cslist[cs->number] = new Chs(cs);
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
  const struct Tab &m_tab;
  uint m_num;
  const char *m_name;
  bool m_pk;
  Type m_type;
  uint m_length;
  uint m_bytelength;  // multiplied by char width
  uint m_attrsize;    // base type size
  uint m_headsize;    // length bytes
  uint m_bytesize;    // full value size
  bool m_nullable;
  const Chs *m_chs;
  Col(const struct Tab &tab, uint num, const char *name, bool pk, Type type,
      uint length, bool nullable, const Chs *chs);
  ~Col();
  bool equal(const Col &col2) const;
  void wellformed(const void *addr) const;
};

Col::Col(const struct Tab &tab, uint num, const char *name, bool pk, Type type,
         uint length, bool nullable, const Chs *chs)
    : m_tab(tab),
      m_num(num),
      m_name(strcpy(new char[strlen(name) + 1], name)),
      m_pk(pk),
      m_type(type),
      m_length(length),
      m_bytelength(length * (chs == 0 ? 1 : chs->m_cs->mbmaxlen)),
      m_attrsize(type == Unsigned
                     ? sizeof(Uint32)
                     : type == Char
                           ? sizeof(char)
                           : type == Varchar
                                 ? sizeof(char)
                                 : type == Longvarchar ? sizeof(char) : ~0),
      m_headsize(type == Unsigned
                     ? 0
                     : type == Char
                           ? 0
                           : type == Varchar ? 1
                                             : type == Longvarchar ? 2 : ~0),
      m_bytesize(m_headsize + m_attrsize * m_bytelength),
      m_nullable(nullable),
      m_chs(chs) {
  // fix long varchar
  if (type == Varchar && m_bytelength > 255) {
    m_type = Longvarchar;
    m_headsize += 1;
    m_bytesize += 1;
  }
}

Col::~Col() { delete[] m_name; }

bool Col::equal(const Col &col2) const {
  return m_type == col2.m_type && m_length == col2.m_length &&
         m_chs == col2.m_chs;
}

void Col::wellformed(const void *addr) const {
  switch (m_type) {
    case Col::Unsigned:
      break;
    case Col::Char: {
      CHARSET_INFO *cs = m_chs->m_cs;
      const char *src = (const char *)addr;
      uint len = m_bytelength;
      int not_used;
      require((*cs->cset->well_formed_len)(cs, src, src + len, 0xffff,
                                           &not_used) == len);
    } break;
    case Col::Varchar: {
      CHARSET_INFO *cs = m_chs->m_cs;
      const uchar *src = (const uchar *)addr;
      const char *ssrc = (const char *)src;
      uint len = src[0];
      int not_used;
      require(len <= m_bytelength);
      require((*cs->cset->well_formed_len)(cs, ssrc + 1, ssrc + 1 + len, 0xffff,
                                           &not_used) == len);
    } break;
    case Col::Longvarchar: {
      CHARSET_INFO *cs = m_chs->m_cs;
      const uchar *src = (const uchar *)addr;
      const char *ssrc = (const char *)src;
      uint len = src[0] + (src[1] << 8);
      int not_used;
      require(len <= m_bytelength);
      require((*cs->cset->well_formed_len)(cs, ssrc + 2, ssrc + 2 + len, 0xffff,
                                           &not_used) == len);
    } break;
    default:
      require(false);
      break;
  }
}

static NdbOut &operator<<(NdbOut &out, const Col &col) {
  out << "col[" << col.m_num << "] " << col.m_name;
  switch (col.m_type) {
    case Col::Unsigned:
      out << " uint";
      break;
    case Col::Char: {
      CHARSET_INFO *cs = col.m_chs->m_cs;
      out << " char(" << col.m_length << "*" << cs->mbmaxlen << ";"
          << cs->m_coll_name << ")";
    } break;
    case Col::Varchar: {
      CHARSET_INFO *cs = col.m_chs->m_cs;
      out << " varchar(" << col.m_length << "*" << cs->mbmaxlen << ";"
          << cs->m_coll_name << ")";
    } break;
    case Col::Longvarchar: {
      CHARSET_INFO *cs = col.m_chs->m_cs;
      out << " longvarchar(" << col.m_length << "*" << cs->mbmaxlen << ";"
          << cs->m_coll_name << ")";
    } break;
    default:
      out << "type" << (int)col.m_type;
      require(false);
      break;
  }
  out << (col.m_pk ? " pk" : "");
  out << (col.m_nullable ? " nullable" : "");
  return out;
}

// ICol - index column

struct ICol {
  const struct ITab &m_itab;
  uint m_num;
  const Col &m_col;
  ICol(const struct ITab &itab, uint num, const Col &col);
  ~ICol();
};

ICol::ICol(const struct ITab &itab, uint num, const Col &col)
    : m_itab(itab), m_num(num), m_col(col) {}

ICol::~ICol() {}

static NdbOut &operator<<(NdbOut &out, const ICol &icol) {
  out << "icol[" << icol.m_num << "] " << icol.m_col;
  return out;
}

// ITab - index

struct ITab {
  enum Type {
    OrderedIndex = NdbDictionary::Index::OrderedIndex,
    UniqueHashIndex = NdbDictionary::Index::UniqueHashIndex
  };
  const struct Tab &m_tab;
  const char *m_name;
  Type m_type;
  uint m_icols;
  const ICol **m_icol;
  uint m_keymask;
  ITab(const struct Tab &tab, const char *name, Type type, uint icols);
  ~ITab();
  void icoladd(uint k, const ICol *icolptr);
};

ITab::ITab(const struct Tab &tab, const char *name, Type type, uint icols)
    : m_tab(tab),
      m_name(strcpy(new char[strlen(name) + 1], name)),
      m_type(type),
      m_icols(icols),
      m_icol(new const ICol *[icols + 1]),
      m_keymask(0) {
  for (uint k = 0; k <= m_icols; k++) m_icol[k] = 0;
}

ITab::~ITab() {
  delete[] m_name;
  for (uint i = 0; i < m_icols; i++) delete m_icol[i];
  delete[] m_icol;
}

void ITab::icoladd(uint k, const ICol *icolptr) {
  require(k == icolptr->m_num && k < m_icols && m_icol[k] == 0);
  m_icol[k] = icolptr;
  m_keymask |= (1 << icolptr->m_col.m_num);
}

static NdbOut &operator<<(NdbOut &out, const ITab &itab) {
  out << "itab " << itab.m_name << " icols=" << itab.m_icols;
  for (uint k = 0; k < itab.m_icols; k++) {
    const ICol &icol = *itab.m_icol[k];
    out << endl << icol;
  }
  return out;
}

// Tab - table

struct Tab {
  const char *m_name;
  uint m_cols;
  const Col **m_col;
  uint m_pkmask;
  uint m_itabs;
  const ITab **m_itab;
  uint m_orderedindexes;
  uint m_hashindexes;
  // pk must contain an Unsigned column
  uint m_keycol;
  void coladd(uint k, Col *colptr);
  void itabadd(uint j, ITab *itab);
  Tab(const char *name, uint cols, uint itabs, uint keycol);
  ~Tab();
};

Tab::Tab(const char *name, uint cols, uint itabs, uint keycol)
    : m_name(strcpy(new char[strlen(name) + 1], name)),
      m_cols(cols),
      m_col(new const Col *[cols + 1]),
      m_pkmask(0),
      m_itabs(itabs),
      m_itab(new const ITab *[itabs + 1]),
      m_orderedindexes(0),
      m_hashindexes(0),
      m_keycol(keycol) {
  for (uint k = 0; k <= cols; k++) m_col[k] = 0;
  for (uint j = 0; j <= itabs; j++) m_itab[j] = 0;
}

Tab::~Tab() {
  delete[] m_name;
  for (uint i = 0; i < m_cols; i++) delete m_col[i];
  delete[] m_col;
  for (uint i = 0; i < m_itabs; i++) delete m_itab[i];
  delete[] m_itab;
}

void Tab::coladd(uint k, Col *colptr) {
  require(k == colptr->m_num && k < m_cols && m_col[k] == 0);
  m_col[k] = colptr;
  if (colptr->m_pk) m_pkmask |= (1 << k);
}

void Tab::itabadd(uint j, ITab *itabptr) {
  require(j < m_itabs && m_itab[j] == 0 && itabptr != 0);
  m_itab[j] = itabptr;
  if (itabptr->m_type == ITab::OrderedIndex)
    m_orderedindexes++;
  else
    m_hashindexes++;
}

static NdbOut &operator<<(NdbOut &out, const Tab &tab) {
  out << "tab " << tab.m_name << " cols=" << tab.m_cols;
  for (uint k = 0; k < tab.m_cols; k++) {
    const Col &col = *tab.m_col[k];
    out << endl << col;
  }
  for (uint i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0) continue;
    const ITab &itab = *tab.m_itab[i];
    out << endl << itab;
  }
  return out;
}

// make table structs

static const Tab **tablist = 0;
static uint tabcount = 0;

static void verifytables() {
  for (uint j = 0; j < tabcount; j++) {
    const Tab *t = tablist[j];
    if (t == 0) continue;
    require(t->m_cols != 0 && t->m_col != 0);
    for (uint k = 0; k < t->m_cols; k++) {
      const Col *c = t->m_col[k];
      require(c != 0 && c->m_num == k);
      require(!(c->m_pk && c->m_nullable));
    }
    require(t->m_col[t->m_cols] == 0);
    {
      require(t->m_keycol < t->m_cols);
      const Col *c = t->m_col[t->m_keycol];
      require(c->m_pk && c->m_type == Col::Unsigned);
    }
    require(t->m_itabs != 0 && t->m_itab != 0);
    for (uint i = 0; i < t->m_itabs; i++) {
      const ITab *x = t->m_itab[i];
      if (x == 0) continue;
      require(x != 0 && x->m_icols != 0 && x->m_icol != 0);
      for (uint k = 0; k < x->m_icols; k++) {
        const ICol *c = x->m_icol[k];
        require(c != 0 && c->m_num == k && c->m_col.m_num < t->m_cols);
        if (x->m_type == ITab::UniqueHashIndex) {
          require(!c->m_col.m_nullable);
        }
      }
    }
    require(t->m_itab[t->m_itabs] == 0);
  }
}

static void makebuiltintables(const Par &par) {
  LL2("makebuiltintables");
  resetcslist();
  tabcount = 3;
  if (tablist == 0) {
    tablist = new const Tab *[tabcount];
    for (uint j = 0; j < tabcount; j++) {
      tablist[j] = 0;
    }
  } else {
    for (uint j = 0; j < tabcount; j++) {
      delete tablist[j];
      tablist[j] = 0;
    }
  }
  // ti0 - basic
  if (usetable(par, 0)) {
    Tab *t = new Tab("ti0", 5, 7, 0);
    // name - pk - type - length - nullable - cs
    t->coladd(0, new Col(*t, 0, "a", 1, Col::Unsigned, 1, 0, 0));
    t->coladd(1, new Col(*t, 1, "b", 0, Col::Unsigned, 1, 1, 0));
    t->coladd(2, new Col(*t, 2, "c", 0, Col::Unsigned, 1, 0, 0));
    t->coladd(3, new Col(*t, 3, "d", 0, Col::Unsigned, 1, 1, 0));
    t->coladd(4, new Col(*t, 4, "e", 0, Col::Unsigned, 1, 0, 0));
    if (useindex(par, 0)) {
      // a
      ITab *x = new ITab(*t, "ti0x0", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      t->itabadd(0, x);
    }
    if (useindex(par, 1)) {
      // b
      ITab *x = new ITab(*t, "ti0x1", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      t->itabadd(1, x);
    }
    if (useindex(par, 2)) {
      // b, c
      ITab *x = new ITab(*t, "ti0x2", ITab::OrderedIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      t->itabadd(2, x);
    }
    if (useindex(par, 3)) {
      // b, e, c, d
      ITab *x = new ITab(*t, "ti0x3", ITab::OrderedIndex, 4);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[4]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[2]));
      x->icoladd(3, new ICol(*x, 3, *t->m_col[3]));
      t->itabadd(3, x);
    }
    if (useindex(par, 4)) {
      // a, c
      ITab *x = new ITab(*t, "ti0z4", ITab::UniqueHashIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      t->itabadd(4, x);
    }
    if (useindex(par, 5)) {
      // a, e
      ITab *x = new ITab(*t, "ti0z5", ITab::UniqueHashIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[4]));
      t->itabadd(5, x);
    }
    tablist[0] = t;
  }
  // ti1 - simple char fields
  if (usetable(par, 1)) {
    Tab *t = new Tab("ti1", 5, 7, 1);
    // name - pk - type - length - nullable - cs
    t->coladd(0, new Col(*t, 0, "a", 0, Col::Unsigned, 1, 0, 0));
    t->coladd(1, new Col(*t, 1, "b", 1, Col::Unsigned, 1, 0, 0));
    t->coladd(2, new Col(*t, 2, "c", 0, Col::Varchar, 20, 0, getcs(par)));
    t->coladd(3, new Col(*t, 3, "d", 0, Col::Char, 5, 0, getcs(par)));
    t->coladd(4, new Col(*t, 4, "e", 0, Col::Longvarchar, 5, 1, getcs(par)));
    if (useindex(par, 0)) {
      // b
      ITab *x = new ITab(*t, "ti1x0", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      t->itabadd(0, x);
    }
    if (useindex(par, 1)) {
      // c, a
      ITab *x = new ITab(*t, "ti1x1", ITab::OrderedIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[2]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[0]));
      t->itabadd(1, x);
    }
    if (useindex(par, 2)) {
      // d
      ITab *x = new ITab(*t, "ti1x2", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[3]));
      t->itabadd(2, x);
    }
    if (useindex(par, 3)) {
      // e, d, c, b
      ITab *x = new ITab(*t, "ti1x3", ITab::OrderedIndex, 4);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[4]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[3]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[2]));
      x->icoladd(3, new ICol(*x, 3, *t->m_col[1]));
      t->itabadd(3, x);
    }
    if (useindex(par, 4)) {
      // a, b
      ITab *x = new ITab(*t, "ti1z4", ITab::UniqueHashIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[1]));
      t->itabadd(4, x);
    }
    if (useindex(par, 5)) {
      // b, c, d
      ITab *x = new ITab(*t, "ti1z5", ITab::UniqueHashIndex, 3);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[3]));
      t->itabadd(5, x);
    }
    tablist[1] = t;
  }
  // ti2 - complex char fields
  if (usetable(par, 2)) {
    Tab *t = new Tab("ti2", 5, 7, 2);
    // name - pk - type - length - nullable - cs
    t->coladd(0, new Col(*t, 0, "a", 1, Col::Char, 31, 0, getcs(par)));
    t->coladd(1, new Col(*t, 1, "b", 0, Col::Char, 4, 1, getcs(par)));
    t->coladd(2, new Col(*t, 2, "c", 1, Col::Unsigned, 1, 0, 0));
    t->coladd(3, new Col(*t, 3, "d", 1, Col::Varchar, 128, 0, getcs(par)));
    t->coladd(4, new Col(*t, 4, "e", 0, Col::Varchar, 7, 0, getcs(par)));
    if (useindex(par, 0)) {
      // a, c, d
      ITab *x = new ITab(*t, "ti2x0", ITab::OrderedIndex, 3);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[3]));
      t->itabadd(0, x);
    }
    if (useindex(par, 1)) {
      // e, d, c, b, a
      ITab *x = new ITab(*t, "ti2x1", ITab::OrderedIndex, 5);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[4]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[3]));
      x->icoladd(2, new ICol(*x, 2, *t->m_col[2]));
      x->icoladd(3, new ICol(*x, 3, *t->m_col[1]));
      x->icoladd(4, new ICol(*x, 4, *t->m_col[0]));
      t->itabadd(1, x);
    }
    if (useindex(par, 2)) {
      // d
      ITab *x = new ITab(*t, "ti2x2", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[3]));
      t->itabadd(2, x);
    }
    if (useindex(par, 3)) {
      // b
      ITab *x = new ITab(*t, "ti2x3", ITab::OrderedIndex, 1);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[1]));
      t->itabadd(3, x);
    }
    if (useindex(par, 4)) {
      // a, c
      ITab *x = new ITab(*t, "ti2z4", ITab::UniqueHashIndex, 2);
      x->icoladd(0, new ICol(*x, 0, *t->m_col[0]));
      x->icoladd(1, new ICol(*x, 1, *t->m_col[2]));
      t->itabadd(4, x);
    }
    if (useindex(par, 5)) {
      // a, c, d, e
      ITab *x = new ITab(*t, "ti2z5", ITab::UniqueHashIndex, 4);
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

static Ndb_cluster_connection *g_ncc = 0;

struct Con {
  Ndb *m_ndb;
  NdbDictionary::Dictionary *m_dic;
  NdbTransaction *m_tx;
  Uint64 m_txid;
  NdbOperation *m_op;
  NdbIndexOperation *m_indexop;
  NdbScanOperation *m_scanop;
  NdbIndexScanOperation *m_indexscanop;
  NdbScanFilter *m_scanfilter;
  enum ScanMode { ScanNo = 0, Committed, Latest, Exclusive };
  ScanMode m_scanmode;
  enum ErrType {
    ErrNone = 0,
    ErrDeadlock = 1,
    ErrNospace = 2,
    ErrLogspace = 4,
    ErrOther = 8
  };
  ErrType m_errtype;
  char m_errname[100];
  Con()
      : m_ndb(0),
        m_dic(0),
        m_tx(0),
        m_txid(0),
        m_op(0),
        m_indexop(0),
        m_scanop(0),
        m_indexscanop(0),
        m_scanfilter(0),
        m_scanmode(ScanNo),
        m_errtype(ErrNone) {}
  ~Con() {
    if (m_tx != 0) closeTransaction();
    delete m_scanfilter;
  }
  int connect();
  void connect(const Con &con);
  void disconnect();
  int startTransaction();
  int getNdbOperation(const Tab &tab);
  int getNdbIndexOperation1(const ITab &itab, const Tab &tab);
  int getNdbIndexOperation(const ITab &itab, const Tab &tab);
  int getNdbScanOperation(const Tab &tab);
  int getNdbIndexScanOperation1(const ITab &itab, const Tab &tab);
  int getNdbIndexScanOperation(const ITab &itab, const Tab &tab);
  int getNdbScanFilter();
  int equal(int num, const char *addr);
  int getValue(int num, NdbRecAttr *&rec);
  int setValue(int num, const char *addr);
  int setBound(int num, int type, const void *value);
  int beginFilter(int group);
  int endFilter();
  int setFilter(int num, int cond, const void *value, uint len);
  int execute(ExecType et);
  int execute(ExecType et, uint &err);
  int readTuple(const Par &par);
  int readTuples(const Par &par);
  int readIndexTuples(const Par &par);
  int executeScan();
  int nextScanResult(bool fetchAllowed);
  int nextScanResult(bool fetchAllowed, uint &err);
  int updateScanTuple(Con &con2);
  int deleteScanTuple(Con &con2);
  void closeScan();
  void closeTransaction();
  const char *errname(uint err);
  void printerror(NdbOut &out);
};

int Con::connect() {
  require(m_ndb == 0);
  m_ndb = new Ndb(g_ncc, "TEST_DB");
  CHKCON(m_ndb->init() == 0, *this);
  CHKCON(m_ndb->waitUntilReady(30) == 0, *this);
  m_tx = 0, m_txid = 0, m_op = 0;
  return 0;
}

void Con::connect(const Con &con) {
  require(m_ndb == 0);
  m_ndb = con.m_ndb;
}

void Con::disconnect() {
  delete m_ndb;
  m_ndb = 0, m_dic = 0, m_tx = 0, m_txid = 0, m_op = 0;
}

int Con::startTransaction() {
  require(m_ndb != 0);
  if (m_tx != 0) closeTransaction();
  CHKCON((m_tx = m_ndb->startTransaction()) != 0, *this);
  m_txid = m_tx->getTransactionId();
  return 0;
}

int Con::getNdbOperation(const Tab &tab) {
  require(m_tx != 0);
  CHKCON((m_op = m_tx->getNdbOperation(tab.m_name)) != 0, *this);
  return 0;
}

int Con::getNdbIndexOperation1(const ITab &itab, const Tab &tab) {
  require(m_tx != 0);
  CHKCON((m_op = m_indexop =
              m_tx->getNdbIndexOperation(itab.m_name, tab.m_name)) != 0,
         *this);
  return 0;
}

int Con::getNdbIndexOperation(const ITab &itab, const Tab &tab) {
  require(m_tx != 0);
  uint tries = 0;
  while (1) {
    if (getNdbIndexOperation1(itab, tab) == 0) break;
    CHK(++tries < 10);
    NdbSleep_MilliSleep(100);
  }
  return 0;
}

int Con::getNdbScanOperation(const Tab &tab) {
  require(m_tx != 0);
  CHKCON((m_op = m_scanop = m_tx->getNdbScanOperation(tab.m_name)) != 0, *this);
  return 0;
}

int Con::getNdbIndexScanOperation1(const ITab &itab, const Tab &tab) {
  require(m_tx != 0);
  CHKCON((m_op = m_scanop = m_indexscanop =
              m_tx->getNdbIndexScanOperation(itab.m_name, tab.m_name)) != 0,
         *this);
  return 0;
}

int Con::getNdbIndexScanOperation(const ITab &itab, const Tab &tab) {
  require(m_tx != 0);
  uint tries = 0;
  while (1) {
    if (getNdbIndexScanOperation1(itab, tab) == 0) break;
    CHK(++tries < 10);
    NdbSleep_MilliSleep(100);
  }
  return 0;
}

int Con::getNdbScanFilter() {
  require(m_tx != 0 && m_scanop != 0);
  delete m_scanfilter;
  m_scanfilter = new NdbScanFilter(m_scanop);
  return 0;
}

int Con::equal(int num, const char *addr) {
  require(m_tx != 0 && m_op != 0);
  CHKCON(m_op->equal(num, addr) == 0, *this);
  return 0;
}

int Con::getValue(int num, NdbRecAttr *&rec) {
  require(m_tx != 0 && m_op != 0);
  CHKCON((rec = m_op->getValue(num, 0)) != 0, *this);
  return 0;
}

int Con::setValue(int num, const char *addr) {
  require(m_tx != 0 && m_op != 0);
  CHKCON(m_op->setValue(num, addr) == 0, *this);
  return 0;
}

int Con::setBound(int num, int type, const void *value) {
  require(m_tx != 0 && m_indexscanop != 0);
  CHKCON(m_indexscanop->setBound(num, type, value) == 0, *this);
  return 0;
}

int Con::beginFilter(int group) {
  require(m_tx != 0 && m_scanfilter != 0);
  CHKCON(m_scanfilter->begin((NdbScanFilter::Group)group) == 0, *this);
  return 0;
}

int Con::endFilter() {
  require(m_tx != 0 && m_scanfilter != 0);
  CHKCON(m_scanfilter->end() == 0, *this);
  return 0;
}

int Con::setFilter(int num, int cond, const void *value, uint len) {
  require(m_tx != 0 && m_scanfilter != 0);
  CHKCON(m_scanfilter->cmp((NdbScanFilter::BinaryCondition)cond, num, value,
                           len) == 0,
         *this);
  return 0;
}

int Con::execute(ExecType et) {
  require(m_tx != 0);
  CHKCON(m_tx->execute(et) == 0, *this);
  return 0;
}

int Con::execute(ExecType et, uint &err) {
  int ret = execute(et);
  // err in: errors to catch, out: error caught
  const uint errin = err;
  err = 0;
  if (ret == -1) {
    if (m_errtype == ErrDeadlock && (errin & ErrDeadlock)) {
      LL3("caught deadlock");
      err = ErrDeadlock;
      ret = 0;
    }
    if (m_errtype == ErrNospace && (errin & ErrNospace)) {
      LL3("caught nospace");
      err = ErrNospace;
      ret = 0;
    }
    if (m_errtype == ErrLogspace && (errin & ErrLogspace)) {
      LL3("caught logspace");
      err = ErrLogspace;
      ret = 0;
    }
  }
  CHK(ret == 0);
  return 0;
}

int Con::readTuple(const Par &par) {
  require(m_tx != 0 && m_op != 0);
  NdbOperation::LockMode lm = par.m_lockmode;
  CHKCON(m_op->readTuple(lm) == 0, *this);
  return 0;
}

int Con::readTuples(const Par &par) {
  require(m_tx != 0 && m_scanop != 0);
  int scan_flags = 0;
  if (par.m_tupscan) scan_flags |= NdbScanOperation::SF_TupScan;
  CHKCON(m_scanop->readTuples(par.m_lockmode, scan_flags, par.m_scanpar,
                              par.m_scanbatch) == 0,
         *this);
  return 0;
}

int Con::readIndexTuples(const Par &par) {
  require(m_tx != 0 && m_indexscanop != 0);
  int scan_flags = 0;
  if (par.m_ordered) scan_flags |= NdbScanOperation::SF_OrderBy;
  if (par.m_descending) scan_flags |= NdbScanOperation::SF_Descending;
  if (par.m_multiRange) {
    scan_flags |= NdbScanOperation::SF_MultiRange;
    scan_flags |= NdbScanOperation::SF_ReadRangeNo;
  }
  CHKCON(m_indexscanop->readTuples(par.m_lockmode, scan_flags, par.m_scanpar,
                                   par.m_scanbatch) == 0,
         *this);
  return 0;
}

int Con::executeScan() {
  CHKCON(m_tx->execute(NoCommit) == 0, *this);
  return 0;
}

int Con::nextScanResult(bool fetchAllowed) {
  int ret;
  require(m_scanop != 0);
  CHKCON((ret = m_scanop->nextResult(fetchAllowed)) != -1, *this);
  require(ret == 0 || ret == 1 || (!fetchAllowed && ret == 2));
  return ret;
}

int Con::nextScanResult(bool fetchAllowed, uint &err) {
  int ret = nextScanResult(fetchAllowed);
  // err in: errors to catch, out: error caught
  const uint errin = err;
  err = 0;
  if (ret == -1) {
    if (m_errtype == ErrDeadlock && (errin & ErrDeadlock)) {
      LL3("caught deadlock");
      err = ErrDeadlock;
      ret = 0;
    }
  }
  CHK(ret == 0 || ret == 1 || (!fetchAllowed && ret == 2));
  return ret;
}

int Con::updateScanTuple(Con &con2) {
  require(con2.m_tx != 0);
  CHKCON((con2.m_op = m_scanop->updateCurrentTuple(con2.m_tx)) != 0, *this);
  con2.m_txid = m_txid;  // in the kernel
  return 0;
}

int Con::deleteScanTuple(Con &con2) {
  require(con2.m_tx != 0);
  CHKCON(m_scanop->deleteCurrentTuple(con2.m_tx) == 0, *this);
  con2.m_txid = m_txid;  // in the kernel
  return 0;
}

void Con::closeScan() {
  require(m_scanop != 0);
  m_scanop->close();
  m_scanop = 0, m_indexscanop = 0;
}

void Con::closeTransaction() {
  require(m_ndb != 0 && m_tx != 0);
  m_ndb->closeTransaction(m_tx);
  m_tx = 0, m_txid = 0, m_op = 0;
  m_scanop = 0, m_indexscanop = 0;
}

const char *Con::errname(uint err) {
  sprintf(m_errname, "0x%x", err);
  if (err & ErrDeadlock) strcat(m_errname, ",deadlock");
  if (err & ErrNospace) strcat(m_errname, ",nospace");
  if (err & ErrLogspace) strcat(m_errname, ",logspace");
  return m_errname;
}

void Con::printerror(NdbOut &out) {
  m_errtype = ErrOther;
  uint any = 0;
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
        if (code == 266 || code == 274 || code == 296 || code == 297 ||
            code == 499 || code == 631)
          m_errtype = ErrDeadlock;
        if (code == 625 || code == 826 || code == 827 || code == 902 ||
            code == 921)
          m_errtype = ErrNospace;
        if (code == 1234 || code == 1220 || code == 410 ||
            code == 1221 ||               // Redo
            code == 923 || code == 1501)  // Undo
          m_errtype = ErrLogspace;
      }
      if (m_op && m_op->getNdbError().code != 0) {
        LL0(++any << " op : error " << m_op->getNdbError());
        die += (code == g_opt.m_die);
      }
    }
  }
  if (!any) {
    LL0("failed but no NDB error code");
  }
  if (die) {
    if (g_opt.m_core) abort();
    exit(1);
  }
}

// dictionary operations

static int invalidateindex(const Par &par, const ITab &itab) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  con.m_ndb->getDictionary()->invalidateIndex(itab.m_name, tab.m_name);
  return 0;
}

static int invalidateindex(Par par) {
  const Tab &tab = par.tab();
  for (uint i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0) continue;
    const ITab &itab = *tab.m_itab[i];
    invalidateindex(par, itab);
  }
  return 0;
}

static int invalidatetable(Par par) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  invalidateindex(par);
  con.m_ndb->getDictionary()->invalidateTable(tab.m_name);
  return 0;
}

static int droptable(Par par) {
  Con &con = par.con();
  const Tab &tab = par.tab();
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

static int createtable(Par par) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  LL3("create table " << tab.m_name);
  LL4(tab);
  NdbDictionary::Table t(tab.m_name);
  if (par.m_fragtype != NdbDictionary::Object::FragUndefined) {
    t.setFragmentType(par.m_fragtype);
  }
  if (par.m_nologging) {
    t.setLogging(false);
  }
  for (uint k = 0; k < tab.m_cols; k++) {
    const Col &col = *tab.m_col[k];
    NdbDictionary::Column c(col.m_name);
    c.setType((NdbDictionary::Column::Type)col.m_type);
    c.setLength(col.m_bytelength);  // for char NDB API uses length in bytes
    c.setPrimaryKey(col.m_pk);
    c.setNullable(col.m_nullable);
    if (col.m_chs != 0) c.setCharset(col.m_chs->m_cs);
    t.addColumn(c);
  }
  con.m_dic = con.m_ndb->getDictionary();
  CHKCON(con.m_dic->createTable(t) == 0, con);
  con.m_dic = 0;
  return 0;
}

static int dropindex(const Par &par, const ITab &itab) {
  Con &con = par.con();
  const Tab &tab = par.tab();
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

static int dropindex(Par par) {
  const Tab &tab = par.tab();
  for (uint i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0) continue;
    const ITab &itab = *tab.m_itab[i];
    CHK(dropindex(par, itab) == 0);
  }
  return 0;
}

static int createindex(const Par &par, const ITab &itab) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  LL3("create index " << itab.m_name);
  LL4(itab);
  NdbDictionary::Index x(itab.m_name);
  x.setTable(tab.m_name);
  x.setType((NdbDictionary::Index::Type)itab.m_type);
  if (par.m_nologging || itab.m_type == ITab::OrderedIndex) {
    x.setLogging(false);
  }
  for (uint k = 0; k < itab.m_icols; k++) {
    const ICol &icol = *itab.m_icol[k];
    const Col &col = icol.m_col;
    x.addColumnName(col.m_name);
  }
  con.m_dic = con.m_ndb->getDictionary();
  CHKCON(con.m_dic->createIndex(x) == 0, con);
  con.m_dic = 0;
  return 0;
}

static int createindex(Par par) {
  const Tab &tab = par.tab();
  for (uint i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0) continue;
    const ITab &itab = *tab.m_itab[i];
    CHK(createindex(par, itab) == 0);
  }
  return 0;
}

// data sets

// Val - typed column value

struct Val {
  const Col &m_col;
  union {
    Uint32 m_uint32;
    uchar *m_char;
    uchar *m_varchar;
    uchar *m_longvarchar;
  };
  bool m_null;
  // construct
  Val(const Col &col);
  ~Val();
  void copy(const Val &val2);
  void copy(const void *addr);
  const void *dataaddr() const;
  void calc(const Par &par, uint i);
  void calckey(const Par &par, uint i);
  void calckeychars(const Par &par, uint i, uint &n, uchar *buf);
  void calcnokey(const Par &par);
  void calcnokeychars(const Par &par, uint &n, uchar *buf);
  // operations
  int setval(const Par &par) const;
  int setval(const Par &par, const ICol &icol) const;
  // compare
  int cmp(const Par &par, const Val &val2) const;
  int cmpchars(const Par &par, const uchar *buf1, uint len1, const uchar *buf2,
               uint len2) const;
  int verify(const Par &par, const Val &val2) const;

 private:
  Val &operator=(const Val &val2);
};

static NdbOut &operator<<(NdbOut &out, const Val &val);

// construct

Val::Val(const Col &col) : m_col(col) {
  switch (col.m_type) {
    case Col::Unsigned:
      m_uint32 = 0x7e7e7e7e;
      break;
    case Col::Char:
      m_char = new uchar[col.m_bytelength];
      memset(m_char, 0x7e, col.m_bytelength);
      break;
    case Col::Varchar:
      m_varchar = new uchar[1 + col.m_bytelength];
      memset(m_char, 0x7e, 1 + col.m_bytelength);
      break;
    case Col::Longvarchar:
      m_longvarchar = new uchar[2 + col.m_bytelength];
      memset(m_char, 0x7e, 2 + col.m_bytelength);
      break;
    default:
      require(false);
      break;
  }
}

Val::~Val() {
  const Col &col = m_col;
  switch (col.m_type) {
    case Col::Unsigned:
      break;
    case Col::Char:
      delete[] m_char;
      break;
    case Col::Varchar:
      delete[] m_varchar;
      break;
    case Col::Longvarchar:
      delete[] m_longvarchar;
      break;
    default:
      require(false);
      break;
  }
}

void Val::copy(const Val &val2) {
  const Col &col = m_col;
  const Col &col2 = val2.m_col;
  require(col.m_type == col2.m_type && col.m_length == col2.m_length);
  if (val2.m_null) {
    m_null = true;
    return;
  }
  copy(val2.dataaddr());
}

void Val::copy(const void *addr) {
  const Col &col = m_col;
  switch (col.m_type) {
    case Col::Unsigned:
      m_uint32 = *(const Uint32 *)addr;
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
      require(false);
      break;
  }
  m_null = false;
}

const void *Val::dataaddr() const {
  const Col &col = m_col;
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
  require(false);
  return 0;
}

void Val::calc(const Par &par, uint i) {
  const Col &col = m_col;
  col.m_pk ? calckey(par, i) : calcnokey(par);
  if (!m_null) col.wellformed(dataaddr());
}

void Val::calckey(const Par &par, uint i) {
  const Col &col = m_col;
  m_null = false;
  switch (col.m_type) {
    case Col::Unsigned:
      m_uint32 = i;
      break;
    case Col::Char: {
      const Chs *chs = col.m_chs;
      CHARSET_INFO *cs = chs->m_cs;
      uint n = 0;
      calckeychars(par, i, n, m_char);
      // extend by appropriate space
      (*cs->cset->fill)(cs, (char *)&m_char[n], col.m_bytelength - n, 0x20);
    } break;
    case Col::Varchar: {
      uint n = 0;
      calckeychars(par, i, n, m_varchar + 1);
      // set length and pad with nulls
      m_varchar[0] = n;
      memset(&m_varchar[1 + n], 0, col.m_bytelength - n);
    } break;
    case Col::Longvarchar: {
      uint n = 0;
      calckeychars(par, i, n, m_longvarchar + 2);
      // set length and pad with nulls
      m_longvarchar[0] = (n & 0xff);
      m_longvarchar[1] = (n >> 8);
      memset(&m_longvarchar[2 + n], 0, col.m_bytelength - n);
    } break;
    default:
      require(false);
      break;
  }
}

void Val::calckeychars(const Par &par, uint i, uint &n, uchar *buf) {
  const Col &col = m_col;
  const Chs *chs = col.m_chs;
  n = 0;
  uint len = 0;
  uint rem = i;
  while (len < col.m_length) {
    if (rem == 0) {
      break;
    }
    uint ix = (rem % maxcharcount);
    rem = (rem / maxcharcount);

    const Chr &chr = chs->m_chr[ix];
    require(n + chr.m_size <= col.m_bytelength);
    memcpy(buf + n, chr.m_bytes, chr.m_size);
    n += chr.m_size;
    len++;
  }
}

void Val::calcnokey(const Par &par) {
  const Col &col = m_col;
  m_null = false;
  if (col.m_nullable && urandom(100) < par.m_pctnull) {
    m_null = true;
    return;
  }
  int r = irandom((par.m_pctrange * par.m_range) / 100);
  if (par.m_bdir != 0 && urandom(10) != 0) {
    if ((r < 0 && par.m_bdir > 0) || (r > 0 && par.m_bdir < 0)) r = -r;
  }
  uint v = par.m_range + r;
  switch (col.m_type) {
    case Col::Unsigned:
      m_uint32 = v;
      break;
    case Col::Char: {
      const Chs *chs = col.m_chs;
      CHARSET_INFO *cs = chs->m_cs;
      uint n = 0;
      calcnokeychars(par, n, m_char);
      // extend by appropriate space
      (*cs->cset->fill)(cs, (char *)&m_char[n], col.m_bytelength - n, 0x20);
    } break;
    case Col::Varchar: {
      uint n = 0;
      calcnokeychars(par, n, m_varchar + 1);
      // set length and pad with nulls
      m_varchar[0] = n;
      memset(&m_varchar[1 + n], 0, col.m_bytelength - n);
    } break;
    case Col::Longvarchar: {
      uint n = 0;
      calcnokeychars(par, n, m_longvarchar + 2);
      // set length and pad with nulls
      m_longvarchar[0] = (n & 0xff);
      m_longvarchar[1] = (n >> 8);
      memset(&m_longvarchar[2 + n], 0, col.m_bytelength - n);
    } break;
    default:
      require(false);
      break;
  }
}

void Val::calcnokeychars(const Par &par, uint &n, uchar *buf) {
  const Col &col = m_col;
  const Chs *chs = col.m_chs;
  n = 0;
  uint len = 0;
  while (len < col.m_length) {
    if (urandom(1 + col.m_bytelength) == 0) {
      break;
    }
    uint half = maxcharcount / 2;
    int r = irandom((par.m_pctrange * half) / 100);
    if (par.m_bdir != 0 && urandom(10) != 0) {
      if ((r < 0 && par.m_bdir > 0) || (r > 0 && par.m_bdir < 0)) r = -r;
    }
    uint i = half + r;
    require(i < maxcharcount);
    const Chr &chr = chs->m_chr[i];
    require(n + chr.m_size <= col.m_bytelength);
    memcpy(buf + n, chr.m_bytes, chr.m_size);
    n += chr.m_size;
    len++;
  }
}

// operations

int Val::setval(const Par &par) const {
  Con &con = par.con();
  const Col &col = m_col;
  if (col.m_pk) {
    require(!m_null);
    const char *addr = (const char *)dataaddr();
    LL5("setval pk [" << col << "] " << *this);
    CHK(con.equal(col.m_num, addr) == 0);
  } else {
    const char *addr = !m_null ? (const char *)dataaddr() : 0;
    LL5("setval non-pk [" << col << "] " << *this);
    CHK(con.setValue(col.m_num, addr) == 0);
  }
  return 0;
}

int Val::setval(const Par &par, const ICol &icol) const {
  Con &con = par.con();
  require(!m_null);
  const char *addr = (const char *)dataaddr();
  LL5("setval key [" << icol << "] " << *this);
  CHK(con.equal(icol.m_num, addr) == 0);
  return 0;
}

// compare

int Val::cmp(const Par &par, const Val &val2) const {
  const Col &col = m_col;
  const Col &col2 = val2.m_col;
  require(col.equal(col2));
  if (m_null || val2.m_null) {
    if (!m_null) return +1;
    if (!val2.m_null) return -1;
    return 0;
  }
  // verify data formats
  col.wellformed(dataaddr());
  col.wellformed(val2.dataaddr());
  // compare
  switch (col.m_type) {
    case Col::Unsigned: {
      if (m_uint32 < val2.m_uint32) return -1;
      if (m_uint32 > val2.m_uint32) return +1;
      return 0;
    } break;
    case Col::Char: {
      uint len1, len2;
      len1 = len2 = col.m_bytelength;
      const Chs *chs = col.m_chs;
      CHARSET_INFO *cs = chs->m_cs;
      if (cs->pad_attribute == NO_PAD) {
        len1 = cs->cset->lengthsp(cs, (const char *)m_char, len1);
        len2 = cs->cset->lengthsp(cs, (const char *)val2.m_char, len2);
      }
      return cmpchars(par, m_char, len1, val2.m_char, len2);
    } break;
    case Col::Varchar: {
      uint len1 = m_varchar[0];
      uint len2 = val2.m_varchar[0];
      return cmpchars(par, m_varchar + 1, len1, val2.m_varchar + 1, len2);
    } break;
    case Col::Longvarchar: {
      uint len1 = m_longvarchar[0] + (m_longvarchar[1] << 8);
      uint len2 = val2.m_longvarchar[0] + (val2.m_longvarchar[1] << 8);
      return cmpchars(par, m_longvarchar + 2, len1, val2.m_longvarchar + 2,
                      len2);
    } break;
    default:
      break;
  }
  require(false);
  return 0;
}

int Val::cmpchars(const Par &par, const uchar *buf1, uint len1,
                  const uchar *buf2, uint len2) const {
  const Col &col = m_col;
  const Chs *chs = col.m_chs;
  CHARSET_INFO *cs = chs->m_cs;
  // Use character set collation-dependent compare function
  const int k = (*cs->coll->strnncollsp)(cs, buf1, len1, buf2, len2);
  return k < 0 ? -1 : k > 0 ? +1 : 0;
}

int Val::verify(const Par &par, const Val &val2) const {
  CHK(cmp(par, val2) == 0);
  return 0;
}

// print

static void printstring(NdbOut &out, const uchar *str, uint len, bool showlen) {
  char buf[4 * NDB_MAX_TUPLE_SIZE];
  char *p = buf;
  *p++ = '[';
  if (showlen) {
    sprintf(p, "%u:", len);
    p += strlen(p);
  }
  for (uint i = 0; i < len; i++) {
    uchar c = str[i];
    if (c == '\\') {
      *p++ = '\\';
      *p++ = c;
    } else if (0x20 <= c && c <= 0x7e) {
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

static NdbOut &operator<<(NdbOut &out, const Val &val) {
  const Col &col = val.m_col;
  if (val.m_null) {
    out << "NULL";
    return out;
  }
  switch (col.m_type) {
    case Col::Unsigned:
      out << val.m_uint32;
      break;
    case Col::Char: {
      uint len = col.m_bytelength;
      printstring(out, val.m_char, len, false);
    } break;
    case Col::Varchar: {
      uint len = val.m_varchar[0];
      printstring(out, val.m_varchar + 1, len, true);
    } break;
    case Col::Longvarchar: {
      uint len = val.m_longvarchar[0] + (val.m_longvarchar[1] << 8);
      printstring(out, val.m_longvarchar + 2, len, true);
    } break;
    default:
      out << "type" << col.m_type;
      require(false);
      break;
  }
  return out;
}

// Row - table tuple

struct Row {
  const Tab &m_tab;
  Val **m_val;
  enum St { StUndef = 0, StDefine = 1, StPrepare = 2, StCommit = 3 };
  enum Op {
    OpNone = 0,
    OpIns = 2,
    OpUpd = 4,
    OpDel = 8,
    OpRead = 16,
    OpReadEx = 32,
    OpReadCom = 64,
    OpDML = 2 | 4 | 8,
    OpREAD = 16 | 32 | 64
  };
  St m_st;
  Op m_op;
  Uint64 m_txid;
  Row *m_bi;
  // construct
  Row(const Tab &tab);
  ~Row();
  void copy(const Row &row2, bool copy_bi);
  void copyval(const Row &row2, uint colmask = ~0);
  void calc(const Par &par, uint i, uint colmask = ~0);
  // operations
  int setval(const Par &par, uint colmask = ~0);
  int setval(const Par &par, const ITab &itab);
  int insrow(const Par &par);
  int updrow(const Par &par);
  int updrow(const Par &par, const ITab &itab);
  int delrow(const Par &par);
  int delrow(const Par &par, const ITab &itab);
  int selrow(const Par &par);
  int selrow(const Par &par, const ITab &itab);
  int setrow(const Par &par);
  // compare
  int cmp(const Par &par, const Row &row2) const;
  int cmp(const Par &par, const Row &row2, const ITab &itab) const;
  int verify(const Par &par, const Row &row2, bool pkonly) const;

 private:
  Row &operator=(const Row &row2);
};

static NdbOut &operator<<(NdbOut &out, const Row *rowp);

static NdbOut &operator<<(NdbOut &out, const Row &row);

// construct

Row::Row(const Tab &tab) : m_tab(tab) {
  m_val = new Val *[tab.m_cols];
  for (uint k = 0; k < tab.m_cols; k++) {
    const Col &col = *tab.m_col[k];
    m_val[k] = new Val(col);
  }
  m_st = StUndef;
  m_op = OpNone;
  m_txid = 0;
  m_bi = 0;
}

Row::~Row() {
  const Tab &tab = m_tab;
  for (uint k = 0; k < tab.m_cols; k++) {
    delete m_val[k];
  }
  delete[] m_val;
  delete m_bi;
}

void Row::copy(const Row &row2, bool copy_bi) {
  const Tab &tab = m_tab;
  copyval(row2);
  m_st = row2.m_st;
  m_op = row2.m_op;
  m_txid = row2.m_txid;
  require(m_bi == 0);
  if (copy_bi && row2.m_bi != 0) {
    m_bi = new Row(tab);
    m_bi->copy(*row2.m_bi, copy_bi);
  }
}

void Row::copyval(const Row &row2, uint colmask) {
  const Tab &tab = m_tab;
  require(&tab == &row2.m_tab);
  for (uint k = 0; k < tab.m_cols; k++) {
    Val &val = *m_val[k];
    const Val &val2 = *row2.m_val[k];
    if ((1 << k) & colmask) val.copy(val2);
  }
}

void Row::calc(const Par &par, uint i, uint colmask) {
  const Tab &tab = m_tab;
  for (uint k = 0; k < tab.m_cols; k++) {
    if ((1 << k) & colmask) {
      Val &val = *m_val[k];
      val.calc(par, i);
    }
  }
}

// operations

int Row::setval(const Par &par, uint colmask) {
  const Tab &tab = m_tab;
  Rsq rsq(tab.m_cols);
  for (uint k = 0; k < tab.m_cols; k++) {
    uint k2 = rsq.next();
    if ((1 << k2) & colmask) {
      const Val &val = *m_val[k2];
      CHK(val.setval(par) == 0);
    }
  }
  return 0;
}

int Row::setval(const Par &par, const ITab &itab) {
  Rsq rsq(itab.m_icols);
  for (uint k = 0; k < itab.m_icols; k++) {
    uint k2 = rsq.next();
    const ICol &icol = *itab.m_icol[k2];
    const Col &col = icol.m_col;
    uint m = col.m_num;
    const Val &val = *m_val[m];
    CHK(val.setval(par, icol) == 0);
  }
  return 0;
}

int Row::insrow(const Par &par) {
  Con &con = par.con();
  const Tab &tab = m_tab;
  CHK(con.getNdbOperation(tab) == 0);
  CHKCON(con.m_op->insertTuple() == 0, con);
  CHK(setval(par, tab.m_pkmask) == 0);
  CHK(setval(par, ~tab.m_pkmask) == 0);
  require(m_st == StUndef);
  m_st = StDefine;
  m_op = OpIns;
  m_txid = con.m_txid;
  return 0;
}

int Row::updrow(const Par &par) {
  Con &con = par.con();
  const Tab &tab = m_tab;
  CHK(con.getNdbOperation(tab) == 0);
  CHKCON(con.m_op->updateTuple() == 0, con);
  CHK(setval(par, tab.m_pkmask) == 0);
  CHK(setval(par, ~tab.m_pkmask) == 0);
  require(m_st == StUndef);
  m_st = StDefine;
  m_op = OpUpd;
  m_txid = con.m_txid;
  return 0;
}

int Row::updrow(const Par &par, const ITab &itab) {
  Con &con = par.con();
  const Tab &tab = m_tab;
  require(itab.m_type == ITab::UniqueHashIndex && &itab.m_tab == &tab);
  CHK(con.getNdbIndexOperation(itab, tab) == 0);
  CHKCON(con.m_op->updateTuple() == 0, con);
  CHK(setval(par, itab) == 0);
  CHK(setval(par, ~tab.m_pkmask) == 0);
  require(m_st == StUndef);
  m_st = StDefine;
  m_op = OpUpd;
  m_txid = con.m_txid;
  return 0;
}

int Row::delrow(const Par &par) {
  Con &con = par.con();
  const Tab &tab = m_tab;
  CHK(con.getNdbOperation(m_tab) == 0);
  CHKCON(con.m_op->deleteTuple() == 0, con);
  CHK(setval(par, tab.m_pkmask) == 0);
  require(m_st == StUndef);
  m_st = StDefine;
  m_op = OpDel;
  m_txid = con.m_txid;
  return 0;
}

int Row::delrow(const Par &par, const ITab &itab) {
  Con &con = par.con();
  const Tab &tab = m_tab;
  require(itab.m_type == ITab::UniqueHashIndex && &itab.m_tab == &tab);
  CHK(con.getNdbIndexOperation(itab, tab) == 0);
  CHKCON(con.m_op->deleteTuple() == 0, con);
  CHK(setval(par, itab) == 0);
  require(m_st == StUndef);
  m_st = StDefine;
  m_op = OpDel;
  m_txid = con.m_txid;
  return 0;
}

int Row::selrow(const Par &par) {
  Con &con = par.con();
  const Tab &tab = m_tab;
  CHK(con.getNdbOperation(m_tab) == 0);
  CHKCON(con.readTuple(par) == 0, con);
  CHK(setval(par, tab.m_pkmask) == 0);
  // TODO state
  return 0;
}

int Row::selrow(const Par &par, const ITab &itab) {
  Con &con = par.con();
  const Tab &tab = m_tab;
  require(itab.m_type == ITab::UniqueHashIndex && &itab.m_tab == &tab);
  CHK(con.getNdbIndexOperation(itab, tab) == 0);
  CHKCON(con.readTuple(par) == 0, con);
  CHK(setval(par, itab) == 0);
  // TODO state
  return 0;
}

int Row::setrow(const Par &par) {
  Con &con = par.con();
  const Tab &tab = m_tab;
  CHK(setval(par, ~tab.m_pkmask) == 0);
  require(m_st == StUndef);
  m_st = StDefine;
  m_op = OpUpd;
  m_txid = con.m_txid;
  return 0;
}

// compare

int Row::cmp(const Par &par, const Row &row2) const {
  const Tab &tab = m_tab;
  require(&tab == &row2.m_tab);
  int c = 0;
  for (uint k = 0; k < tab.m_cols; k++) {
    const Val &val = *m_val[k];
    const Val &val2 = *row2.m_val[k];
    if ((c = val.cmp(par, val2)) != 0) break;
  }
  return c;
}

int Row::cmp(const Par &par, const Row &row2, const ITab &itab) const {
  const Tab &tab = m_tab;
  int c = 0;
  for (uint i = 0; i < itab.m_icols; i++) {
    const ICol &icol = *itab.m_icol[i];
    const Col &col = icol.m_col;
    uint k = col.m_num;
    require(k < tab.m_cols);
    const Val &val = *m_val[k];
    const Val &val2 = *row2.m_val[k];
    if ((c = val.cmp(par, val2)) != 0) break;
  }
  return c;
}

int Row::verify(const Par &par, const Row &row2, bool pkonly) const {
  const Tab &tab = m_tab;
  const Row &row1 = *this;
  require(&row1.m_tab == &row2.m_tab);
  for (uint k = 0; k < tab.m_cols; k++) {
    const Col &col = row1.m_val[k]->m_col;
    if (!pkonly || col.m_pk) {
      const Val &val1 = *row1.m_val[k];
      const Val &val2 = *row2.m_val[k];
      CHK(val1.verify(par, val2) == 0);
    }
  }
  return 0;
}

// print

static NdbOut &operator<<(NdbOut &out, const Row::St st) {
  if (st == Row::StUndef)
    out << "StUndef";
  else if (st == Row::StDefine)
    out << "StDefine";
  else if (st == Row::StPrepare)
    out << "StPrepare";
  else if (st == Row::StCommit)
    out << "StCommit";
  else
    out << "st=" << st;
  return out;
}

static NdbOut &operator<<(NdbOut &out, const Row::Op op) {
  if (op == Row::OpNone)
    out << "OpNone";
  else if (op == Row::OpIns)
    out << "OpIns";
  else if (op == Row::OpUpd)
    out << "OpUpd";
  else if (op == Row::OpDel)
    out << "OpDel";
  else if (op == Row::OpRead)
    out << "OpRead";
  else if (op == Row::OpReadEx)
    out << "OpReadEx";
  else if (op == Row::OpReadCom)
    out << "OpReadCom";
  else
    out << "op=" << op;
  return out;
}

static NdbOut &operator<<(NdbOut &out, const Row *rowp) {
  if (rowp == 0)
    out << "[null]";
  else
    out << *rowp;
  return out;
}

static NdbOut &operator<<(NdbOut &out, const Row &row) {
  const Tab &tab = row.m_tab;
  out << "[";
  for (uint i = 0; i < tab.m_cols; i++) {
    if (i > 0) out << " ";
    out << *row.m_val[i];
  }
  out << " " << row.m_st;
  out << " " << row.m_op;
  out << " " << HEX(row.m_txid);
  if (row.m_bi != 0) out << " " << row.m_bi;
  out << "]";
  return out;
}

// Set - set of table tuples

struct Set {
  const Tab &m_tab;
  const uint m_rows;
  Row **m_row;
  uint *m_rowkey;  // maps row number (from 0) in scan to tuple key
  Row *m_keyrow;
  NdbRecAttr **m_rec;
  // construct
  Set(const Tab &tab, uint rows);
  ~Set();
  void reset();
  bool compat(const Par &par, uint i, const Row::Op op) const;
  void push(uint i);
  void copyval(uint i, uint colmask = ~0);  // from bi
  void calc(const Par &par, uint i, uint colmask = ~0);
  uint count() const;
  const Row *getrow(uint i, bool dirty = false) const;
  int setrow(uint i, const Row *src, bool force = false);
  // transaction
  void post(const Par &par, ExecType et);
  // operations
  int insrow(const Par &par, uint i);
  int updrow(const Par &par, uint i);
  int updrow(const Par &par, const ITab &itab, uint i);
  int delrow(const Par &par, uint i);
  int delrow(const Par &par, const ITab &itab, uint i);
  int selrow(const Par &par, const Row &keyrow);
  int selrow(const Par &par, const ITab &itab, const Row &keyrow);
  int setrow(const Par &par, uint i);
  int getval(const Par &par);
  int getkey(const Par &par, uint *i);
  int putval(uint i, bool force, uint n = ~0);
  // compare
  void sort(const Par &par, const ITab &itab);
  int verify(const Par &par, const Set &set2, bool pkonly,
             bool dirty = false) const;
  int verifyorder(const Par &par, const ITab &itab, bool descending) const;
  // protect structure
  NdbMutex *m_mutex;
  void lock() const { NdbMutex_Lock(m_mutex); }
  void unlock() const { NdbMutex_Unlock(m_mutex); }

 private:
  void sort(const Par &par, const ITab &itab, uint lo, uint hi);
  Set &operator=(const Set &set2);
};

// construct

Set::Set(const Tab &tab, uint rows) : m_tab(tab), m_rows(rows) {
  m_row = new Row *[m_rows];
  for (uint i = 0; i < m_rows; i++) {
    m_row[i] = 0;
  }
  m_rowkey = new uint[m_rows];
  for (uint n = 0; n < m_rows; n++) {
    m_rowkey[n] = ~0;
  }
  m_keyrow = new Row(tab);
  m_rec = new NdbRecAttr *[tab.m_cols];
  for (uint k = 0; k < tab.m_cols; k++) {
    m_rec[k] = 0;
  }
  m_mutex = NdbMutex_Create();
  require(m_mutex != 0);
}

Set::~Set() {
  for (uint i = 0; i < m_rows; i++) {
    delete m_row[i];
  }
  delete[] m_row;
  delete[] m_rowkey;
  delete m_keyrow;
  delete[] m_rec;
  NdbMutex_Destroy(m_mutex);
}

void Set::reset() {
  for (uint i = 0; i < m_rows; i++) {
    delete m_row[i];
    m_row[i] = 0;
  }
}

// this sucks
bool Set::compat(const Par &par, uint i, const Row::Op op) const {
  Con &con = par.con();
  int ret = -1;
  int place = 0;
  do {
    const Row *rowp = getrow(i);
    if (rowp == 0) {
      ret = op == Row::OpIns;
      place = 1;
      break;
    }
    const Row &row = *rowp;
    if (!(op & Row::OpREAD)) {
      if (row.m_st == Row::StDefine || row.m_st == Row::StPrepare) {
        require(row.m_op & Row::OpDML);
        require(row.m_txid != 0);
        if (con.m_txid != row.m_txid) {
          ret = false;
          place = 2;
          break;
        }
        if (row.m_op != Row::OpDel) {
          ret = op == Row::OpUpd || op == Row::OpDel;
          place = 3;
          break;
        }
        ret = op == Row::OpIns;
        place = 4;
        break;
      }
      if (row.m_st == Row::StCommit) {
        require(row.m_op == Row::OpNone);
        require(row.m_txid == 0);
        ret = op == Row::OpUpd || op == Row::OpDel;
        place = 5;
        break;
      }
    }
    if (op & Row::OpREAD) {
      bool dirty = con.m_txid != row.m_txid &&
                   par.m_lockmode == NdbOperation::LM_CommittedRead;
      const Row *rowp2 = getrow(i, dirty);
      if (rowp2 == 0 || rowp2->m_op == Row::OpDel) {
        ret = false;
        place = 6;
        break;
      }
      ret = true;
      place = 7;
      break;
    }
  } while (0);
  LL4("compat ret=" << ret << " place=" << place);
  require(ret == false || ret == true);
  return ret;
}

void Set::push(uint i) {
  const Tab &tab = m_tab;
  require(i < m_rows);
  Row *bi = m_row[i];
  m_row[i] = new Row(tab);
  Row &row = *m_row[i];
  row.m_bi = bi;
  if (bi != 0) row.copyval(*bi);
}

void Set::copyval(uint i, uint colmask) {
  require(m_row[i] != 0);
  Row &row = *m_row[i];
  require(row.m_bi != 0);
  row.copyval(*row.m_bi, colmask);
}

void Set::calc(const Par &par, uint i, uint colmask) {
  require(m_row[i] != 0);
  Row &row = *m_row[i];
  row.calc(par, i, colmask);
}

uint Set::count() const {
  uint count = 0;
  for (uint i = 0; i < m_rows; i++) {
    if (m_row[i] != 0) count++;
  }
  return count;
}

const Row *Set::getrow(uint i, bool dirty) const {
  require(i < m_rows);
  const Row *rowp = m_row[i];
  if (dirty) {
    while (rowp != 0) {
      bool b1 = rowp->m_op == Row::OpNone;
      bool b2 = rowp->m_st == Row::StCommit;
      require(b1 == b2);
      if (b1) {
        require(rowp->m_bi == 0);
        break;
      }
      rowp = rowp->m_bi;
    }
  }
  return rowp;
}

int Set::setrow(uint i, const Row *src, bool force) {
  require(i < m_rows);
  if (m_row[i] != 0) {
    if (!force) return -1;
    delete m_row[i];
    m_row[i] = 0;
  }

  Row *newRow = new Row(src->m_tab);
  newRow->copy(*src, true);
  m_row[i] = newRow;
  return 0;
}

// transaction

void Set::post(const Par &par, ExecType et) {
  LL4("post");
  Con &con = par.con();
  require(con.m_txid != 0);
  uint i;
  for (i = 0; i < m_rows; i++) {
    Row *rowp = m_row[i];
    if (rowp == 0) {
      LL5("skip " << i << " " << rowp);
      continue;
    }
    if (rowp->m_st == Row::StCommit) {
      require(rowp->m_op == Row::OpNone);
      require(rowp->m_txid == 0);
      require(rowp->m_bi == 0);
      LL5("skip committed " << i << " " << rowp);
      continue;
    }
    require(rowp->m_st == Row::StDefine || rowp->m_st == Row::StPrepare);
    require(rowp->m_txid != 0);
    if (con.m_txid != rowp->m_txid) {
      LL5("skip txid " << i << " " << HEX(con.m_txid) << " " << rowp);
      continue;
    }
    // TODO read ops
    require(rowp->m_op & Row::OpDML);
    LL4("post BEFORE " << rowp);
    if (et == NoCommit) {
      if (rowp->m_st == Row::StDefine) {
        rowp->m_st = Row::StPrepare;
        Row *bi = rowp->m_bi;
        while (bi != 0 && bi->m_st == Row::StDefine) {
          bi->m_st = Row::StPrepare;
          bi = bi->m_bi;
        }
      }
    } else if (et == Commit) {
      if (rowp->m_op != Row::OpDel) {
        rowp->m_st = Row::StCommit;
        rowp->m_op = Row::OpNone;
        rowp->m_txid = 0;
        delete rowp->m_bi;
        rowp->m_bi = 0;
      } else {
        delete rowp;
        rowp = 0;
      }
    } else if (et == Rollback) {
      while (rowp != 0 && rowp->m_st != Row::StCommit) {
        Row *tmp = rowp;
        rowp = rowp->m_bi;
        tmp->m_bi = 0;
        delete tmp;
      }
    } else {
      require(false);
    }
    m_row[i] = rowp;
    LL4("post AFTER " << rowp);
  }
}

// operations

int Set::insrow(const Par &par, uint i) {
  require(m_row[i] != 0);
  Row &row = *m_row[i];
  CHK(row.insrow(par) == 0);
  return 0;
}

int Set::updrow(const Par &par, uint i) {
  require(m_row[i] != 0);
  Row &row = *m_row[i];
  CHK(row.updrow(par) == 0);
  return 0;
}

int Set::updrow(const Par &par, const ITab &itab, uint i) {
  require(m_row[i] != 0);
  Row &row = *m_row[i];
  CHK(row.updrow(par, itab) == 0);
  return 0;
}

int Set::delrow(const Par &par, uint i) {
  require(m_row[i] != 0);
  Row &row = *m_row[i];
  CHK(row.delrow(par) == 0);
  return 0;
}

int Set::delrow(const Par &par, const ITab &itab, uint i) {
  require(m_row[i] != 0);
  Row &row = *m_row[i];
  CHK(row.delrow(par, itab) == 0);
  return 0;
}

int Set::selrow(const Par &par, const Row &keyrow) {
  const Tab &tab = par.tab();
  LL5("selrow " << tab.m_name << " keyrow " << keyrow);
  m_keyrow->copyval(keyrow, tab.m_pkmask);
  CHK(m_keyrow->selrow(par) == 0);
  CHK(getval(par) == 0);
  return 0;
}

int Set::selrow(const Par &par, const ITab &itab, const Row &keyrow) {
  LL5("selrow " << itab.m_name << " keyrow " << keyrow);
  m_keyrow->copyval(keyrow, itab.m_keymask);
  CHK(m_keyrow->selrow(par, itab) == 0);
  CHK(getval(par) == 0);
  return 0;
}

int Set::setrow(const Par &par, uint i) {
  require(m_row[i] != 0);
  CHK(m_row[i]->setrow(par) == 0);
  return 0;
}

int Set::getval(const Par &par) {
  Con &con = par.con();
  const Tab &tab = m_tab;
  Rsq rsq1(tab.m_cols);
  for (uint k = 0; k < tab.m_cols; k++) {
    uint k2 = rsq1.next();
    CHK(con.getValue(k2, m_rec[k2]) == 0);
  }
  return 0;
}

int Set::getkey(const Par &par, uint *i) {
  const Tab &tab = m_tab;
  uint k = tab.m_keycol;
  require(m_rec[k] != 0);
  const char *aRef = m_rec[k]->aRef();
  Uint32 key = *(const Uint32 *)aRef;
  LL5("getkey: " << key);
  CHK(key < m_rows);
  *i = key;
  return 0;
}

int Set::putval(uint i, bool force, uint n) {
  const Tab &tab = m_tab;
  LL4("putval key=" << i << " row=" << n << " old=" << m_row[i]);
  CHK(i < m_rows);
  if (m_row[i] != 0) {
    require(force);
    delete m_row[i];
    m_row[i] = 0;
  }
  m_row[i] = new Row(tab);
  Row &row = *m_row[i];
  for (uint k = 0; k < tab.m_cols; k++) {
    Val &val = *row.m_val[k];
    NdbRecAttr *rec = m_rec[k];
    require(rec != 0);
    if (rec->isNULL()) {
      val.m_null = true;
      continue;
    }
    const char *aRef = m_rec[k]->aRef();
    val.copy(aRef);
    val.m_null = false;
  }
  if (n != (uint)~0) {
    CHK(n < m_rows);
    m_rowkey[n] = i;
  }
  return 0;
}

// compare

void Set::sort(const Par &par, const ITab &itab) {
  if (m_rows != 0) sort(par, itab, 0, m_rows - 1);
}

void Set::sort(const Par &par, const ITab &itab, uint lo, uint hi) {
  require(lo < m_rows && hi < m_rows && lo <= hi);
  Row *const p = m_row[lo];
  uint i = lo;
  uint j = hi;
  while (i < j) {
    while (i < j && m_row[j]->cmp(par, *p, itab) >= 0) j--;
    if (i < j) {
      m_row[i] = m_row[j];
      i++;
    }
    while (i < j && m_row[i]->cmp(par, *p, itab) <= 0) i++;
    if (i < j) {
      m_row[j] = m_row[i];
      j--;
    }
  }
  m_row[i] = p;
  if (lo < i) sort(par, itab, lo, i - 1);
  if (hi > i) sort(par, itab, i + 1, hi);
}

/*
 * set1 (self) is from dml and can contain un-committed operations.
 * set2 is from read and contains no operations.  "dirty" applies
 * to set1: false = use latest row, true = use committed row.
 */
int Set::verify(const Par &par, const Set &set2, bool pkonly,
                bool dirty) const {
  const Set &set1 = *this;
  require(&set1.m_tab == &set2.m_tab && set1.m_rows == set2.m_rows);
  LL3("verify dirty:" << dirty);
  for (uint i = 0; i < set1.m_rows; i++) {
    // the row versions we actually compare
    const Row *row1p = set1.getrow(i, dirty);
    const Row *row2p = set2.getrow(i);
    bool ok = true;
    int place = 0;
    if (row1p == 0) {
      if (row2p != 0) {
        ok = false;
        place = 1;
      }
    } else {
      Row::Op op1 = row1p->m_op;
      if (op1 != Row::OpDel) {
        if (row2p == 0) {
          ok = false;
          place = 2;
        } else if (row1p->verify(par, *row2p, pkonly) == -1) {
          ok = false;
          place = 3;
        }
      } else if (row2p != 0) {
        ok = false;
        place = 4;
      }
    }
    if (!ok) {
      LL1("verify " << i << " failed at " << place);
      LL1("row1 " << row1p);
      LL1("row2 " << row2p);
      CHK(false);
    }
  }
  return 0;
}

int Set::verifyorder(const Par &par, const ITab &itab, bool descending) const {
  for (uint n = 0; n < m_rows; n++) {
    uint i2 = m_rowkey[n];
    if (i2 == (uint)~0) break;
    if (n == 0) continue;
    uint i1 = m_rowkey[n - 1];
    require(m_row[i1] != 0 && m_row[i2] != 0);
    const Row &row1 = *m_row[i1];
    const Row &row2 = *m_row[i2];
    bool ok;
    if (!descending)
      ok = (row1.cmp(par, row2, itab) <= 0);
    else
      ok = (row1.cmp(par, row2, itab) >= 0);

    if (!ok) {
      LL1("verifyorder " << n << " failed");
      LL1("row1 " << row1);
      LL1("row2 " << row2);
      CHK(false);
    }
  }
  return 0;
}

// print

#if 0
static NdbOut&
operator<<(NdbOut& out, const Set& set)
{
  for (uint i = 0; i < set.m_rows; i++) {
    const Row& row = *set.m_row[i];
    if (i > 0)
      out << endl;
    out << row;
  }
  return out;
}
#endif

// BVal - range scan bound

struct BVal : public Val {
  const ICol &m_icol;
  int m_type;
  BVal(const ICol &icol);
  int setbnd(const Par &par) const;
  int setflt(const Par &par) const;
};

BVal::BVal(const ICol &icol) : Val(icol.m_col), m_icol(icol) {}

int BVal::setbnd(const Par &par) const {
  Con &con = par.con();
  require(g_compare_null || !m_null);
  const char *addr = !m_null ? (const char *)dataaddr() : 0;
  const ICol &icol = m_icol;
  CHK(con.setBound(icol.m_num, m_type, addr) == 0);
  return 0;
}

int BVal::setflt(const Par &par) const {
  static uint index_bound_to_filter_bound[5] = {
      NdbScanFilter::COND_GE, NdbScanFilter::COND_GT, NdbScanFilter::COND_LE,
      NdbScanFilter::COND_LT, NdbScanFilter::COND_EQ};
  Con &con = par.con();
  require(g_compare_null || !m_null);
  const char *addr = !m_null ? (const char *)dataaddr() : 0;
  const ICol &icol = m_icol;
  const Col &col = icol.m_col;
  uint length = col.m_bytesize;
  uint cond = index_bound_to_filter_bound[m_type];
  CHK(con.setFilter(col.m_num, cond, addr, length) == 0);
  return 0;
}

static NdbOut &operator<<(NdbOut &out, const BVal &bval) {
  const ICol &icol = bval.m_icol;
  const Col &col = icol.m_col;
  const Val &val = bval;
  out << "type=" << bval.m_type;
  out << " icol=" << icol.m_num;
  out << " col=" << col.m_num << "," << col.m_name;
  out << " value=" << val;
  return out;
}

// BSet - set of bounds

struct BSet {
  const Tab &m_tab;
  const ITab &m_itab;
  uint m_alloc;
  uint m_bvals;
  BVal **m_bval;
  BSet(const Tab &tab, const ITab &itab);
  ~BSet();
  void reset();
  void calc(Par par);
  void calcpk(const Par &par, uint i);
  int setbnd(const Par &par) const;
  int setflt(const Par &par) const;
  void filter(const Par &par, const Set &set, Set &set2) const;
};

BSet::BSet(const Tab &tab, const ITab &itab)
    : m_tab(tab), m_itab(itab), m_alloc(2 * itab.m_icols), m_bvals(0) {
  m_bval = new BVal *[m_alloc];
  for (uint i = 0; i < m_alloc; i++) {
    m_bval[i] = 0;
  }
}

BSet::~BSet() {
  for (uint i = 0; i < m_alloc; i++) {
    delete m_bval[i];
  }
  delete[] m_bval;
}

void BSet::reset() {
  while (m_bvals > 0) {
    uint i = --m_bvals;
    delete m_bval[i];
    m_bval[i] = 0;
  }
}

void BSet::calc(Par par) {
  const ITab &itab = m_itab;
  par.m_pctrange = par.m_pctbrange;
  reset();
  for (uint k = 0; k < itab.m_icols; k++) {
    const ICol &icol = *itab.m_icol[k];
    for (uint i = 0; i <= 1; i++) {
      if (m_bvals == 0 && urandom(100) == 0) return;
      if (m_bvals != 0 && urandom(3) == 0) return;
      require(m_bvals < m_alloc);
      BVal &bval = *new BVal(icol);
      m_bval[m_bvals++] = &bval;
      bval.m_null = false;
      uint sel;
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
      if (k + 1 < itab.m_icols) bval.m_type = 4;
      if (!g_compare_null) par.m_pctnull = 0;
      if (bval.m_type == 0 || bval.m_type == 1) par.m_bdir = -1;
      if (bval.m_type == 2 || bval.m_type == 3) par.m_bdir = +1;
      do {
        bval.calcnokey(par);
        if (i == 1) {
          require(m_bvals >= 2);
          const BVal &bv1 = *m_bval[m_bvals - 2];
          const BVal &bv2 = *m_bval[m_bvals - 1];
          if (bv1.cmp(par, bv2) > 0 && urandom(100) != 0) continue;
        }
      } while (0);
      // equality bound only once
      if (bval.m_type == 4) break;
    }
  }
}

void BSet::calcpk(const Par &par, uint i) {
  const ITab &itab = m_itab;
  reset();
  for (uint k = 0; k < itab.m_icols; k++) {
    const ICol &icol = *itab.m_icol[k];
    const Col &col = icol.m_col;
    require(col.m_pk);
    require(m_bvals < m_alloc);
    BVal &bval = *new BVal(icol);
    m_bval[m_bvals++] = &bval;
    bval.m_type = 4;
    bval.calc(par, i);
  }
}

int BSet::setbnd(const Par &par) const {
  if (m_bvals != 0) {
    Rsq rsq1(m_bvals);
    for (uint j = 0; j < m_bvals; j++) {
      uint j2 = rsq1.next();
      const BVal &bval = *m_bval[j2];
      CHK(bval.setbnd(par) == 0);
    }
  }
  return 0;
}

int BSet::setflt(const Par &par) const {
  Con &con = par.con();
  CHK(con.getNdbScanFilter() == 0);
  CHK(con.beginFilter(NdbScanFilter::AND) == 0);
  if (m_bvals != 0) {
    Rsq rsq1(m_bvals);
    for (uint j = 0; j < m_bvals; j++) {
      uint j2 = rsq1.next();
      const BVal &bval = *m_bval[j2];
      CHK(bval.setflt(par) == 0);
    }
    // duplicate
    if (urandom(5) == 0) {
      uint j3 = urandom(m_bvals);
      const BVal &bval = *m_bval[j3];
      CHK(bval.setflt(par) == 0);
    }
  }
  CHK(con.endFilter() == 0);
  return 0;
}

void BSet::filter(const Par &par, const Set &set, Set &set2) const {
  const Tab &tab = m_tab;
  const ITab &itab = m_itab;
  require(&tab == &set2.m_tab && set.m_rows == set2.m_rows);
  require(set2.count() == 0);
  for (uint i = 0; i < set.m_rows; i++) {
    set.lock();
    do {
      if (set.m_row[i] == 0) {
        break;
      }
      const Row &row = *set.m_row[i];
      if (!g_store_null_key) {
        bool ok1 = false;
        for (uint k = 0; k < itab.m_icols; k++) {
          const ICol &icol = *itab.m_icol[k];
          const Col &col = icol.m_col;
          const Val &val = *row.m_val[col.m_num];
          if (!val.m_null) {
            ok1 = true;
            break;
          }
        }
        if (!ok1) break;
      }
      bool ok2 = true;
      for (uint j = 0; j < m_bvals; j++) {
        const BVal &bval = *m_bval[j];
        const ICol &icol = bval.m_icol;
        const Col &col = icol.m_col;
        const Val &val = *row.m_val[col.m_num];
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
          require(false);
        }
        if (!ok2) break;
      }
      if (!ok2) break;
      require(set2.m_row[i] == 0);
      set2.m_row[i] = new Row(tab);
      Row &row2 = *set2.m_row[i];
      row2.copy(row, true);
    } while (0);
    set.unlock();
  }
}

static NdbOut &operator<<(NdbOut &out, const BSet &bset) {
  out << "bounds=" << bset.m_bvals;
  for (uint j = 0; j < bset.m_bvals; j++) {
    const BVal &bval = *bset.m_bval[j];
    out << " [bound " << j << ": " << bval << "]";
  }
  return out;
}

// pk operations

static int pkinsert(Par par) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  LL3("pkinsert " << tab.m_name);
  CHK(con.startTransaction() == 0);
  uint batch = 0;
  for (uint j = 0; j < par.m_rows; j++) {
    uint j2 = !par.m_randomkey ? j : urandom(par.m_rows);
    uint i = thrrow(par, j2);
    set.lock();
    if (!set.compat(par, i, Row::OpIns)) {
      LL3("pkinsert SKIP " << i << " " << set.getrow(i));
      set.unlock();
    } else {
      set.push(i);
      set.calc(par, i);
      CHK(set.insrow(par, i) == 0);
      set.unlock();
      LL4("pkinsert key=" << i << " " << set.getrow(i));
      batch++;
    }
    bool lastbatch = (batch != 0 && j + 1 == par.m_rows);
    if (batch == par.m_batch || lastbatch) {
      uint err = par.m_catcherr;
      ExecType et = !randompct(par.m_abortpct) ? Commit : Rollback;
      CHK(con.execute(et, err) == 0);
      set.lock();
      set.post(par, !err ? et : Rollback);
      set.unlock();
      if (err) {
        LL1("pkinsert key=" << i << " stop on " << con.errname(err));
        break;
      }
      batch = 0;
      if (!lastbatch) {
        con.closeTransaction();
        CHK(con.startTransaction() == 0);
      }
    }
  }
  con.closeTransaction();
  return 0;
}

static int pkupdate(Par par) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  LL3("pkupdate " << tab.m_name);
  CHK(con.startTransaction() == 0);
  uint batch = 0;
  for (uint j = 0; j < par.m_rows; j++) {
    uint j2 = !par.m_randomkey ? j : urandom(par.m_rows);
    uint i = thrrow(par, j2);
    set.lock();
    if (!set.compat(par, i, Row::OpUpd)) {
      LL3("pkupdate SKIP " << i << " " << set.getrow(i));
      set.unlock();
    } else {
      set.push(i);
      set.copyval(i, tab.m_pkmask);
      set.calc(par, i, ~tab.m_pkmask);
      CHK(set.updrow(par, i) == 0);
      set.unlock();
      LL4("pkupdate key=" << i << " " << set.getrow(i));
      batch++;
    }
    bool lastbatch = (batch != 0 && j + 1 == par.m_rows);
    if (batch == par.m_batch || lastbatch) {
      uint err = par.m_catcherr;
      ExecType et = !randompct(par.m_abortpct) ? Commit : Rollback;
      CHK(con.execute(et, err) == 0);
      set.lock();
      set.post(par, !err ? et : Rollback);
      set.unlock();
      if (et == Commit) {
        LL4("pkupdate key committed = " << i << " " << set.getrow(i));
      }
      if (err) {
        LL1("pkupdate key=" << i << ": stop on " << con.errname(err));
        break;
      }
      batch = 0;
      if (!lastbatch) {
        con.closeTransaction();
        CHK(con.startTransaction() == 0);
      }
    }
  }
  con.closeTransaction();
  return 0;
}

static int pkdelete(Par par) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  LL3("pkdelete " << tab.m_name);
  CHK(con.startTransaction() == 0);
  uint batch = 0;
  for (uint j = 0; j < par.m_rows; j++) {
    uint j2 = !par.m_randomkey ? j : urandom(par.m_rows);
    uint i = thrrow(par, j2);
    set.lock();
    if (!set.compat(par, i, Row::OpDel)) {
      LL3("pkdelete SKIP " << i << " " << set.getrow(i));
      set.unlock();
    } else {
      set.push(i);
      set.copyval(i, tab.m_pkmask);
      CHK(set.delrow(par, i) == 0);
      set.unlock();
      LL4("pkdelete key=" << i << " " << set.getrow(i));
      batch++;
    }
    bool lastbatch = (batch != 0 && j + 1 == par.m_rows);
    if (batch == par.m_batch || lastbatch) {
      uint err = par.m_catcherr;
      ExecType et = !randompct(par.m_abortpct) ? Commit : Rollback;
      CHK(con.execute(et, err) == 0);
      set.lock();
      set.post(par, !err ? et : Rollback);
      set.unlock();
      if (err) {
        LL1("pkdelete key=" << i << " stop on " << con.errname(err));
        break;
      }
      batch = 0;
      if (!lastbatch) {
        con.closeTransaction();
        CHK(con.startTransaction() == 0);
      }
    }
  }
  con.closeTransaction();
  return 0;
}

#if 0
static int
pkread(const Par& par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("pkread " << tab.m_name << " verify=" << par.m_verify);
  // expected
  const Set& set1 = set;
  Set set2(tab, set.m_rows);
  for (uint i = 0; i < set.m_rows; i++) {
    set.lock();
    // TODO lock mode
    if (!set.compat(par, i, Row::OpREAD)) {
      LL3("pkread SKIP " << i << " " << set.getrow(i));
      set.unlock();
      continue;
    }
    set.unlock();
    CHK(con.startTransaction() == 0);
    CHK(set2.selrow(par, *set1.m_row[i]) == 0);
    CHK(con.execute(Commit) == 0);
    uint i2 = (uint)-1;
    CHK(set2.getkey(par, &i2) == 0 && i == i2);
    CHK(set2.putval(i, false) == 0);
    LL4("row " << set2.count() << " " << set2.getrow(i));
    con.closeTransaction();
  }
  if (par.m_verify)
    CHK(set1.verify(par, set2, false) == 0);
  return 0;
}
#endif

static int pkreadfast(const Par &par, uint count) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  const Set &set = par.set();
  LL3("pkfast " << tab.m_name);
  Row keyrow(tab);
  // not batched on purpose
  for (uint j = 0; j < count; j++) {
    uint i = urandom(set.m_rows);
    require(set.compat(par, i, Row::OpREAD));
    CHK(con.startTransaction() == 0);
    // define key
    keyrow.calc(par, i);
    CHK(keyrow.selrow(par) == 0);
    NdbRecAttr *rec;
    // get 1st column
    CHK(con.getValue((Uint32)0, rec) == 0);
    CHK(con.execute(Commit) == 0);
    con.closeTransaction();
  }
  return 0;
}

// hash index operations

static int hashindexupdate(const Par &par, const ITab &itab) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  LL3("hashindexupdate " << itab.m_name);
  CHK(con.startTransaction() == 0);
  uint batch = 0;
  for (uint j = 0; j < par.m_rows; j++) {
    uint j2 = !par.m_randomkey ? j : urandom(par.m_rows);
    uint i = thrrow(par, j2);
    set.lock();
    if (!set.compat(par, i, Row::OpUpd)) {
      LL3("hashindexupdate SKIP " << i << " " << set.getrow(i));
      set.unlock();
    } else {
      // table pk and index key are not updated
      set.push(i);
      uint keymask = tab.m_pkmask | itab.m_keymask;
      set.copyval(i, keymask);
      set.calc(par, i, ~keymask);
      CHK(set.updrow(par, itab, i) == 0);
      set.unlock();
      LL4("hashindexupdate " << i << " " << set.getrow(i));
      batch++;
    }
    bool lastbatch = (batch != 0 && j + 1 == par.m_rows);
    if (batch == par.m_batch || lastbatch) {
      uint err = par.m_catcherr;
      ExecType et = !randompct(par.m_abortpct) ? Commit : Rollback;
      CHK(con.execute(et, err) == 0);
      set.lock();
      set.post(par, !err ? et : Rollback);
      set.unlock();
      if (err) {
        LL1("hashindexupdate " << i << " stop on " << con.errname(err));
        break;
      }
      batch = 0;
      if (!lastbatch) {
        con.closeTransaction();
        CHK(con.startTransaction() == 0);
      }
    }
  }
  con.closeTransaction();
  return 0;
}

static int hashindexdelete(const Par &par, const ITab &itab) {
  Con &con = par.con();
  Set &set = par.set();
  LL3("hashindexdelete " << itab.m_name);
  CHK(con.startTransaction() == 0);
  uint batch = 0;
  for (uint j = 0; j < par.m_rows; j++) {
    uint j2 = !par.m_randomkey ? j : urandom(par.m_rows);
    uint i = thrrow(par, j2);
    set.lock();
    if (!set.compat(par, i, Row::OpDel)) {
      LL3("hashindexdelete SKIP " << i << " " << set.getrow(i));
      set.unlock();
    } else {
      set.push(i);
      set.copyval(i, itab.m_keymask);
      CHK(set.delrow(par, itab, i) == 0);
      set.unlock();
      LL4("hashindexdelete " << i << " " << set.getrow(i));
      batch++;
    }
    bool lastbatch = (batch != 0 && j + 1 == par.m_rows);
    if (batch == par.m_batch || lastbatch) {
      uint err = par.m_catcherr;
      ExecType et = !randompct(par.m_abortpct) ? Commit : Rollback;
      CHK(con.execute(et, err) == 0);
      set.lock();
      set.post(par, !err ? et : Rollback);
      set.unlock();
      if (err) {
        LL1("hashindexdelete " << i << " stop on " << con.errname(err));
        break;
      }
      batch = 0;
      if (!lastbatch) {
        con.closeTransaction();
        CHK(con.startTransaction() == 0);
      }
    }
  }
  con.closeTransaction();
  return 0;
}

static int hashindexread(const Par &par, const ITab &itab) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  LL3("hashindexread " << itab.m_name << " verify=" << par.m_verify);
  // expected
  const Set &set1 = set;
  Set set2(tab, set.m_rows);
  for (uint i = 0; i < set.m_rows; i++) {
    set.lock();
    // TODO lock mode
    if (!set.compat(par, i, Row::OpREAD)) {
      LL3("hashindexread SKIP " << i << " " << set.getrow(i));
      set.unlock();
      continue;
    }
    set.unlock();
    CHK(con.startTransaction() == 0);
    CHK(set2.selrow(par, itab, *set1.m_row[i]) == 0);
    CHK(con.execute(Commit) == 0);
    uint i2 = (uint)-1;
    CHK(set2.getkey(par, &i2) == 0 && i == i2);
    CHK(set2.putval(i, false) == 0);
    LL4("row " << set2.count() << " " << *set2.m_row[i]);
    con.closeTransaction();
  }
  if (par.m_verify) CHK(set1.verify(par, set2, false) == 0);
  return 0;
}

// scan read

static int scanreadtable(const Par &par) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  const Set &set = par.set();
  // expected
  const Set &set1 = set;
  LL3("scanreadtable " << tab.m_name << " lockmode=" << par.m_lockmode
                       << " tupscan=" << par.m_tupscan << " expect="
                       << set1.count() << " verify=" << par.m_verify);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.readTuples(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  uint n = 0;
  while (1) {
    int ret;
    uint err = par.m_catcherr;
    CHK((ret = con.nextScanResult(true, err)) == 0 || ret == 1);
    if (ret == 1) break;
    if (err) {
      LL1("scanreadtable stop on " << con.errname(err));
      break;
    }
    uint i = (uint)-1;
    CHK(set2.getkey(par, &i) == 0);
    CHK(set2.putval(i, false, n) == 0);
    LL4("row " << n << " " << *set2.m_row[i]);
    n++;
  }
  con.closeTransaction();
  if (par.m_verify) CHK(set1.verify(par, set2, false) == 0);
  LL3("scanreadtable " << tab.m_name << " done rows=" << n);
  return 0;
}

static int scanreadtablefast(const Par &par, uint countcheck) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  LL3("scanfast " << tab.m_name);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.readTuples(par) == 0);
  // get 1st column
  NdbRecAttr *rec;
  CHK(con.getValue((Uint32)0, rec) == 0);
  CHK(con.executeScan() == 0);
  uint count = 0;
  while (1) {
    int ret;
    CHK((ret = con.nextScanResult(true)) == 0 || ret == 1);
    if (ret == 1) break;
    count++;
  }
  con.closeTransaction();
  CHK(count == countcheck);
  return 0;
}

// try to get interesting bounds
static void calcscanbounds(const Par &par, const ITab &itab, BSet &bset,
                           const Set &set, Set &set1) {
  while (true) {
    bset.calc(par);
    bset.filter(par, set, set1);
    uint n = set1.count();
    // prefer proper subset
    if (0 < n && n < set.m_rows) break;
    if (urandom(5) == 0) break;
    set1.reset();
  }
}

static int scanreadindex(const Par &par, const ITab &itab, BSet &bset,
                         bool calc) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  const Set &set = par.set();
  Set set1(tab, set.m_rows);
  if (calc) {
    calcscanbounds(par, itab, bset, set, set1);
  } else {
    bset.filter(par, set, set1);
  }
  LL3("scanreadindex " << itab.m_name << " " << bset << " lockmode="
                       << par.m_lockmode << " expect=" << set1.count()
                       << " ordered=" << par.m_ordered << " descending="
                       << par.m_descending << " verify=" << par.m_verify);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbIndexScanOperation(itab, tab) == 0);
  CHK(con.readIndexTuples(par) == 0);
  CHK(bset.setbnd(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  uint n = 0;

  bool debugging_skip_put_dup_check = false;

  while (1) {
    int ret;
    uint err = par.m_catcherr;
    CHK((ret = con.nextScanResult(true, err)) == 0 || ret == 1);
    if (ret == 1) break;
    if (err) {
      LL1("scanreadindex stop on " << con.errname(err));
      break;
    }
    uint i = (uint)-1;
    CHK(set2.getkey(par, &i) == 0);

    // Debug code to track down 'putval()' of duplicate records.
    if (!par.m_dups && set2.m_row[i] != nullptr) {
      Row tmp(tab);
      for (uint k = 0; k < tab.m_cols; k++) {
        Val &val = *tmp.m_val[k];
        NdbRecAttr *rec = set2.m_rec[k];
        require(rec != 0);
        if (rec->isNULL()) {
          val.m_null = true;
          continue;
        }
        const char *aRef = rec->aRef();
        val.copy(aRef);
        val.m_null = false;
      }

      LL0("scanreadindex " << itab.m_name << " " << bset << " lockmode="
                           << par.m_lockmode << " expect=" << set1.count()
                           << " ordered=" << par.m_ordered << " descending="
                           << par.m_descending << " verify=" << par.m_verify);
      LL0("Table : " << itab.m_tab);
      LL0("Index : " << itab);
      LL0("scanreadindex read duplicate, total rows expected in set: "
          << set1.count());
      LL0("  read so far: " << set2.count());
      LL0("  nextScanResult returned: " << ret << ", err: " << err);
      LL0("");

      LL0("  Row key existed, key=" << i << " row#" << n << "\n     old="
                                    << *set2.m_row[i] << "\n     new=" << tmp);

      if (!debugging_skip_put_dup_check) {
        LL0("First duplicate in scan, test will fail, check for "
            "further duplicates / result set incorrectness.");
        /* Only need expected set in first duplicate case */
        LL0("------------ Set expected -----------");
        for (uint i = 0; i < set1.m_rows; i++) {
          Row *row = set1.m_row[i];
          if (row != nullptr) {
            LL0("Row#" << i << ", " << *row);
          }
        }
      }

      LL0("------------ Set read ---------------");
      for (uint i = 0; i < set2.m_rows; i++) {
        Row *row = set2.m_row[i];
        if (row != nullptr) {
          LL0("Row#" << i << ", " << *row);
        }
      }
      LL0("-------------------------------------");

      LL0("scanreadindex read duplicate, total rows expected in set: "
          << set1.count());
      LL0("  read so far: " << set2.count());
      LL0("  nextScanResult returned: " << ret << ", err: " << err);
      LL0("");

      LL0("  Row key existed, key=" << i << " row#" << n << "\n     old="
                                    << *set2.m_row[i] << "\n     new=" << tmp);
      debugging_skip_put_dup_check = true;
    }

    CHK(set2.putval(i, (par.m_dups || debugging_skip_put_dup_check), n) == 0);
    LL4("key " << i << " row " << n << " " << *set2.m_row[i]);
    n++;
  }
  if (debugging_skip_put_dup_check) {
    LL0("Warning : there were duplicates - test wil fail, "
        "but checking results for whole scan first");
  }
  con.closeTransaction();
  if (par.m_verify) {
    CHK(set1.verify(par, set2, false) == 0);
    if (par.m_ordered) CHK(set2.verifyorder(par, itab, par.m_descending) == 0);
  }
  CHK(!debugging_skip_put_dup_check);  // Fail here
  LL3("scanreadindex " << itab.m_name << " done rows=" << n);
  return 0;
}

static int scanreadindexmrr(Par par, const ITab &itab, int numBsets) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  const Set &set = par.set();

  /* Create space for different sets of bounds, expected results and
   * results
   * Calculate bounds and the sets of rows which would result
   */
  BSet **boundSets;
  Set **expectedResults;
  Set **actualResults;
  uint *setSizes;

  boundSets = new BSet *[numBsets];
  expectedResults = new Set *[numBsets];
  actualResults = new Set *[numBsets];
  setSizes = new uint[numBsets];

  for (int n = 0; n < numBsets; n++) {
    CHK((boundSets[n] = new BSet(tab, itab)) != NULL);
    CHK((expectedResults[n] = new Set(tab, set.m_rows)) != NULL);
    CHK((actualResults[n] = new Set(tab, set.m_rows)) != NULL);
    setSizes[n] = 0;

    Set &results = *expectedResults[n];
    /* Calculate some scan bounds which are selective */
    do {
      results.reset();
      calcscanbounds(par, itab, *boundSets[n], set, results);
    } while ((*boundSets[n]).m_bvals == 0);
  }

  /* Define scan with bounds */
  LL3("scanreadindexmrr " << itab.m_name << " ranges= " << numBsets
                          << " lockmode=" << par.m_lockmode << " ordered="
                          << par.m_ordered << " descending=" << par.m_descending
                          << " verify=" << par.m_verify);
  Set set2(tab, set.m_rows);
  /* Multirange + Read range number for this scan */
  par.m_multiRange = true;
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbIndexScanOperation(itab, tab) == 0);
  CHK(con.readIndexTuples(par) == 0);
  /* Set the bounds */
  for (int n = 0; n < numBsets; n++) {
    CHK(boundSets[n]->setbnd(par) == 0);
    int res = con.m_indexscanop->end_of_bound(n);
    if (res != 0) {
      LL1("end_of_bound error : " << con.m_indexscanop->getNdbError().code);
      CHK(false);
    }
  }
  set2.getval(par);
  CHK(con.executeScan() == 0);
  int rows_received = 0;
  while (1) {
    int ret;
    uint err = par.m_catcherr;
    CHK((ret = con.nextScanResult(true, err)) == 0 || ret == 1);
    if (ret == 1) break;
    if (err) {
      LL1("scanreadindexmrr stop on " << con.errname(err));
      break;
    }
    uint i = (uint)-1;
    /* Put value into set2 temporarily */
    CHK(set2.getkey(par, &i) == 0);
    CHK(set2.putval(i, false, -1) == 0);

    /* Now move it to the correct set, based on the range no */
    int rangeNum = con.m_indexscanop->get_range_no();
    CHK(rangeNum < numBsets);
    CHK(set2.m_row[i] != NULL);
    if (setSizes[rangeNum] != actualResults[rangeNum]->count()) {
      /* Debug info */
      LL0("scanreadindexmrr failure");
      LL0("scanreadindexmrr "
          << itab.m_name << " ranges= " << numBsets
          << " lockmode=" << par.m_lockmode << " ordered=" << par.m_ordered
          << " descending=" << par.m_descending << " verify=" << par.m_verify);
      LL0("Table : " << itab.m_tab);
      LL0("Index : " << itab);
      LL0("rows_received " << rows_received << " i " << i);
      LL0("rangeNum " << rangeNum << " setSizes[rangeNum] "
                      << setSizes[rangeNum]
                      << " actualResults[rangeNum]->count() "
                      << actualResults[rangeNum]->count());
      LL0("Row : " << set2.m_row[i]);

      for (int range = 0; range < numBsets; range++) {
        LL0("--------Range # " << range << "--------");
        LL0("  Bounds : " << *boundSets[range]);
        int expectedCount = expectedResults[range]->count();
        LL0("  Expected rows : " << expectedCount);
        for (int e = 0; e < expectedCount; e++) {
          Row *r = expectedResults[range]->m_row[e];
          if (r != NULL) {
            LL0("Row#" << e << ", " << *r);
          }
        }
        int actualCount = actualResults[range]->count();
        LL0("  Received rows so far : " << actualCount);
        for (int a = 0; a < actualCount; a++) {
          Row *r = actualResults[range]->m_row[a];
          if (r != NULL) {
            LL0("Row#" << a << ", " << *r);
          }
        }
      }
      LL0("------End of ranges------");
    }
    /* Get rowNum based on what's in the set already (slow) */
    CHK(setSizes[rangeNum] == actualResults[rangeNum]->count());
    int rowNum = setSizes[rangeNum];
    if (actualResults[rangeNum]->m_row[i] == 0 || !par.m_dups) {
      setSizes[rangeNum]++;
    } else {
      LL1("Row with same PK exists, can happen with updates to index"
          " columns while scanning");
    }
    CHK((uint)rowNum < set2.m_rows);
    actualResults[rangeNum]->m_row[i] = set2.m_row[i];
    actualResults[rangeNum]->m_rowkey[rowNum] = i;
    LL4("range " << rangeNum << " key " << i << " row " << rowNum << " "
                 << *set2.m_row[i]);
    set2.m_row[i] = 0;
    rows_received++;
  }
  con.closeTransaction();

  /* Verify that each set has the expected rows, and optionally, that
   * they're ordered
   */
  if (par.m_verify) {
    LL4("Verifying " << numBsets << " sets, " << rows_received << " rows");
    for (int n = 0; n < numBsets; n++) {
      LL5("Set " << n << " of " << expectedResults[n]->count() << " rows");
      CHK(expectedResults[n]->verify(par, *actualResults[n], false) == 0);
      if (par.m_ordered) {
        LL5("Verifying ordering");
        CHK(actualResults[n]->verifyorder(par, itab, par.m_descending) == 0);
      }
    }
  }

  /* Cleanup */
  for (int n = 0; n < numBsets; n++) {
    boundSets[n]->reset();
    delete boundSets[n];
    delete expectedResults[n];
    delete actualResults[n];
  }
  delete[] boundSets;
  delete[] expectedResults;
  delete[] actualResults;
  delete[] setSizes;

  LL3("scanreadindexmrr " << itab.m_name << " done rows=" << rows_received);
  return 0;
}

static int scanreadindexfast(const Par &par, const ITab &itab, const BSet &bset,
                             uint countcheck) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  LL3("scanfast " << itab.m_name << " " << bset);
  LL4(bset);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbIndexScanOperation(itab, tab) == 0);
  CHK(con.readIndexTuples(par) == 0);
  CHK(bset.setbnd(par) == 0);
  // get 1st column
  NdbRecAttr *rec;
  CHK(con.getValue((Uint32)0, rec) == 0);
  CHK(con.executeScan() == 0);
  uint count = 0;
  while (1) {
    int ret;
    CHK((ret = con.nextScanResult(true)) == 0 || ret == 1);
    if (ret == 1) break;
    count++;
  }
  con.closeTransaction();
  CHK(count == countcheck);
  return 0;
}

static int scanreadfilter(const Par &par, const ITab &itab, BSet &bset,
                          bool calc) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  const Set &set = par.set();
  Set set1(tab, set.m_rows);
  if (calc) {
    calcscanbounds(par, itab, bset, set, set1);
  } else {
    bset.filter(par, set, set1);
  }
  LL3("scanfilter " << itab.m_name << " " << bset << " lockmode="
                    << par.m_lockmode << " expect=" << set1.count()
                    << " verify=" << par.m_verify);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.readTuples(par) == 0);
  CHK(bset.setflt(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  uint n = 0;
  while (1) {
    int ret;
    uint err = par.m_catcherr;
    CHK((ret = con.nextScanResult(true, err)) == 0 || ret == 1);
    if (ret == 1) break;
    if (err) {
      LL1("scanfilter stop on " << con.errname(err));
      break;
    }
    uint i = (uint)-1;
    CHK(set2.getkey(par, &i) == 0);
    CHK(set2.putval(i, par.m_dups, n) == 0);
    LL4("key " << i << " row " << n << " " << *set2.m_row[i]);
    n++;
  }
  con.closeTransaction();
  if (par.m_verify) {
    CHK(set1.verify(par, set2, false) == 0);
  }
  LL3("scanfilter " << itab.m_name << " done rows=" << n);
  return 0;
}

static int scanreadindex(const Par &par, const ITab &itab) {
  const Tab &tab = par.tab();
  for (uint i = 0; i < par.m_ssloop; i++) {
    if (itab.m_type == ITab::OrderedIndex) {
      BSet bset(tab, itab);
      CHK(scanreadfilter(par, itab, bset, true) == 0);
      /* Single range or Multi range scan */
      if (randompct(g_opt.m_pctmrr))
        CHK(scanreadindexmrr(par, itab, 1 + urandom(g_opt.m_mrrmaxrng - 1)) ==
            0);
      else
        CHK(scanreadindex(par, itab, bset, true) == 0);
    }
  }
  return 0;
}

static int scanreadindex(const Par &par) {
  const Tab &tab = par.tab();
  for (uint i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0) continue;
    const ITab &itab = *tab.m_itab[i];
    if (itab.m_type == ITab::OrderedIndex) {
      CHK(scanreadindex(par, itab) == 0);
    } else {
      CHK(hashindexread(par, itab) == 0);
    }
  }
  return 0;
}

#if 0
static int
scanreadall(Par par)
{
  CHK(scanreadtable(par) == 0);
  CHK(scanreadindex(par) == 0);
  return 0;
}
#endif

// timing scans

static int timescantable(Par par) {
  par.tmr().on();
  CHK(scanreadtablefast(par, par.m_totrows) == 0);
  par.tmr().off(par.set().m_rows);
  return 0;
}

static int timescanpkindex(Par par) {
  const Tab &tab = par.tab();
  const ITab &itab = *tab.m_itab[0];  // 1st index is on PK
  BSet bset(tab, itab);
  par.tmr().on();
  CHK(scanreadindexfast(par, itab, bset, par.m_totrows) == 0);
  par.tmr().off(par.set().m_rows);
  return 0;
}

static int timepkreadtable(Par par) {
  par.tmr().on();
  uint count = par.m_samples;
  if (count == 0) count = par.m_totrows;
  CHK(pkreadfast(par, count) == 0);
  par.tmr().off(count);
  return 0;
}

static int timepkreadindex(Par par) {
  const Tab &tab = par.tab();
  const ITab &itab = *tab.m_itab[0];  // 1st index is on PK
  BSet bset(tab, itab);
  uint count = par.m_samples;
  if (count == 0) count = par.m_totrows;
  par.tmr().on();
  for (uint j = 0; j < count; j++) {
    uint i = urandom(par.m_totrows);
    bset.calcpk(par, i);
    CHK(scanreadindexfast(par, itab, bset, 1) == 0);
  }
  par.tmr().off(count);
  return 0;
}

// scan update

static int scanupdatetable(Par par) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  LL3("scanupdatetable " << tab.m_name);
  Set set2(tab, set.m_rows);
  par.m_lockmode = NdbOperation::LM_Exclusive;
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.readTuples(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  uint count = 0;
  // updating trans
  Con con2;
  con2.connect(con);
  CHK(con2.startTransaction() == 0);
  uint batch = 0;
  while (1) {
    int ret;
    uint32 err = par.m_catcherr;
    CHK((ret = con.nextScanResult(true, err)) != -1);
    if (ret != 0) break;
    if (err) {
      LL1("scanupdatetable [scan] stop on " << con.errname(err));
      break;
    }
    if (par.m_scanstop != 0 && urandom(par.m_scanstop) == 0) {
      con.closeScan();
      break;
    }
    while (1) {
      uint i = (uint)-1;
      CHK(set2.getkey(par, &i) == 0);
      set.lock();
      if (!set.compat(par, i, Row::OpUpd)) {
        LL3("scanupdatetable SKIP " << i << " " << set.getrow(i));
      } else {
        CHKTRY(set2.putval(i, false) == 0, set.unlock());
        CHKTRY(con.updateScanTuple(con2) == 0, set.unlock());
        Par par2 = par;
        par2.m_con = &con2;
        set.push(i);
        set.calc(par, i, ~tab.m_pkmask);
        CHKTRY(set.setrow(par2, i) == 0, set.unlock());
        LL4("scanupdatetable " << i << " " << set.getrow(i));
        batch++;
      }
      set.unlock();
      CHK((ret = con.nextScanResult(false)) != -1);
      bool lastbatch = (batch != 0 && ret != 0);
      if (batch == par.m_batch || lastbatch) {
        uint err = par.m_catcherr;
        ExecType et = Commit;
        CHK(con2.execute(et, err) == 0);
        set.lock();
        set.post(par, !err ? et : Rollback);
        set.unlock();
        if (err) {
          LL1("scanupdatetable [update] stop on " << con2.errname(err));
          goto out;
        }
        LL4("scanupdatetable committed batch");
        count += batch;
        batch = 0;
        con2.closeTransaction();
        CHK(con2.startTransaction() == 0);
      }
      if (ret != 0) break;
    }
  }
out:
  con2.closeTransaction();
  LL3("scanupdatetable " << tab.m_name << " rows updated=" << count);
  con.closeTransaction();
  return 0;
}

static int scanupdateindex(Par par, const ITab &itab, BSet &bset, bool calc) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  // expected
  Set set1(tab, set.m_rows);
  if (calc) {
    calcscanbounds(par, itab, bset, set, set1);
  } else {
    bset.filter(par, set, set1);
  }
  LL3("scanupdateindex " << itab.m_name << " " << bset << " expect="
                         << set1.count() << " ordered=" << par.m_ordered
                         << " descending=" << par.m_descending
                         << " verify=" << par.m_verify);
  Set set2(tab, set.m_rows);
  par.m_lockmode = NdbOperation::LM_Exclusive;
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbIndexScanOperation(itab, tab) == 0);
  CHK(con.readTuples(par) == 0);
  CHK(bset.setbnd(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  uint count = 0;
  // updating trans
  Con con2;
  con2.connect(con);
  CHK(con2.startTransaction() == 0);
  uint batch = 0;
  while (1) {
    int ret;
    uint err = par.m_catcherr;
    CHK((ret = con.nextScanResult(true, err)) != -1);
    if (ret != 0) break;
    if (err) {
      LL1("scanupdateindex [scan] stop on " << con.errname(err));
      break;
    }
    if (par.m_scanstop != 0 && urandom(par.m_scanstop) == 0) {
      con.closeScan();
      break;
    }
    while (1) {
      uint i = (uint)-1;
      CHK(set2.getkey(par, &i) == 0);
      set.lock();
      if (!set.compat(par, i, Row::OpUpd)) {
        LL4("scanupdateindex SKIP " << set.getrow(i));
      } else {
        CHKTRY(set2.putval(i, par.m_dups) == 0, set.unlock());
        CHKTRY(con.updateScanTuple(con2) == 0, set.unlock());
        Par par2 = par;
        par2.m_con = &con2;
        set.push(i);
        uint colmask = !par.m_noindexkeyupdate ? ~0 : ~itab.m_keymask;
        set.calc(par, i, colmask);
        CHKTRY(set.setrow(par2, i) == 0, set.unlock());
        LL4("scanupdateindex " << i << " " << set.getrow(i));
        batch++;
      }
      set.unlock();
      CHK((ret = con.nextScanResult(false)) != -1);
      bool lastbatch = (batch != 0 && ret != 0);
      if (batch == par.m_batch || lastbatch) {
        uint err = par.m_catcherr;
        ExecType et = Commit;
        CHK(con2.execute(et, err) == 0);
        set.lock();
        set.post(par, !err ? et : Rollback);
        set.unlock();
        if (err) {
          LL1("scanupdateindex [update] stop on " << con2.errname(err));
          goto out;
        }
        LL4("scanupdateindex committed batch");
        count += batch;
        batch = 0;
        con2.closeTransaction();
        CHK(con2.startTransaction() == 0);
      }
      if (ret != 0) break;
    }
  }
out:
  con2.closeTransaction();
  if (par.m_verify) {
    CHK(set1.verify(par, set2, true) == 0);
    if (par.m_ordered) CHK(set2.verifyorder(par, itab, par.m_descending) == 0);
  }
  LL3("scanupdateindex " << itab.m_name << " rows updated=" << count);
  con.closeTransaction();
  return 0;
}

static int scanupdateindex(const Par &par, const ITab &itab) {
  const Tab &tab = par.tab();
  for (uint i = 0; i < par.m_ssloop; i++) {
    if (itab.m_type == ITab::OrderedIndex) {
      BSet bset(tab, itab);
      CHK(scanupdateindex(par, itab, bset, true) == 0);
    } else {
      CHK(hashindexupdate(par, itab) == 0);
    }
  }
  return 0;
}

static int scanupdateindex(const Par &par) {
  const Tab &tab = par.tab();
  for (uint i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0) continue;
    const ITab &itab = *tab.m_itab[i];
    CHK(scanupdateindex(par, itab) == 0);
  }
  return 0;
}

#if 0
static int
scanupdateall(const Par& par)
{
  CHK(scanupdatetable(par) == 0);
  CHK(scanupdateindex(par) == 0);
  return 0;
}
#endif

// medium level routines

static int readverifyfull(Par par) {
  if (par.m_noverify) return 0;
  par.m_verify = true;
  if (par.m_abortpct != 0) {
    LL2("skip verify in this version");  // implement in 5.0 version
    par.m_verify = false;
  }
  par.m_lockmode = NdbOperation::LM_CommittedRead;
  const Tab &tab = par.tab();
  if (par.m_no == 0) {
    // thread 0 scans table
    CHK(scanreadtable(par) == 0);
    // once more via tup scan
    par.m_tupscan = true;
    CHK(scanreadtable(par) == 0);
  }
  // each thread scans different indexes
  for (uint i = 0; i < tab.m_itabs; i++) {
    if ((i % par.m_usedthreads) != par.m_no) continue;
    if (tab.m_itab[i] == 0) continue;
    const ITab &itab = *tab.m_itab[i];
    if (itab.m_type == ITab::OrderedIndex) {
      BSet bset(tab, itab);
      CHK(scanreadindex(par, itab, bset, false) == 0);
    } else {
      CHK(hashindexread(par, itab) == 0);
    }
  }
  return 0;
}

static int readverifyindex(Par par) {
  if (par.m_noverify) return 0;
  par.m_verify = true;
  par.m_lockmode = NdbOperation::LM_CommittedRead;
  uint sel = urandom(10);
  if (sel < 9) {
    par.m_ordered = true;
    par.m_descending = (sel < 5);
  }
  CHK(scanreadindex(par) == 0);
  return 0;
}

static int pkops(Par par) {
  const Tab &tab = par.tab();
  par.m_randomkey = true;
  for (uint i = 0; i < par.m_ssloop; i++) {
    uint j = 0;
    while (j < tab.m_itabs) {
      if (tab.m_itab[j] != 0) {
        const ITab &itab = *tab.m_itab[j];
        if (itab.m_type == ITab::UniqueHashIndex && urandom(5) == 0) break;
      }
      j++;
    }
    uint sel = urandom(10);
    if (par.m_slno % 2 == 0) {
      // favor insert
      if (sel < 8) {
        CHK(pkinsert(par) == 0);
      } else if (sel < 9) {
        if (j == tab.m_itabs)
          CHK(pkupdate(par) == 0);
        else {
          const ITab &itab = *tab.m_itab[j];
          CHK(hashindexupdate(par, itab) == 0);
        }
      } else {
        if (j == tab.m_itabs)
          CHK(pkdelete(par) == 0);
        else {
          const ITab &itab = *tab.m_itab[j];
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
          const ITab &itab = *tab.m_itab[j];
          CHK(hashindexupdate(par, itab) == 0);
        }
      } else {
        if (j == tab.m_itabs)
          CHK(pkdelete(par) == 0);
        else {
          const ITab &itab = *tab.m_itab[j];
          CHK(hashindexdelete(par, itab) == 0);
        }
      }
    }
  }
  return 0;
}

static int pkupdatescanread(Par par) {
  par.m_dups = true;
  par.m_catcherr |= Con::ErrDeadlock;
  uint sel = urandom(10);
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

static int mixedoperations(Par par) {
  par.m_dups = true;
  par.m_catcherr |= Con::ErrDeadlock;
  par.m_scanstop = par.m_totrows;  // randomly close scans
  uint sel = urandom(10);
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

static int parallelorderedupdate(Par par) {
  const Tab &tab = par.tab();
  uint k = 0;
  for (uint i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0) continue;
    const ITab &itab = *tab.m_itab[i];
    if (itab.m_type != ITab::OrderedIndex) continue;
    // cannot sync threads yet except via subloop
    if (k++ == par.m_slno % tab.m_orderedindexes) {
      LL3("parallelorderedupdate: " << itab.m_name);
      par.m_noindexkeyupdate = true;
      par.m_ordered = true;
      par.m_descending = (par.m_slno != 0);
      par.m_dups = false;
      par.m_verify = true;
      BSet bset(tab, itab);  // empty bounds
      // prefer empty bounds
      uint sel = urandom(10);
      CHK(scanupdateindex(par, itab, bset, sel < 2) == 0);
    }
  }
  return 0;
}

static int pkupdateindexbuild(Par par) {
  if (par.m_no == 0) {
    NdbSleep_MilliSleep(10 + urandom(100));
    CHK(createindex(par) == 0);
  } else {
    NdbSleep_MilliSleep(10 + urandom(100));
    par.m_randomkey = true;
    CHK(pkupdate(par) == 0);
  }
  return 0;
}

// savepoint tests (single thread for now)

struct Spt {
  enum Res { Committed, Latest, Deadlock };
  bool m_same;  // same transaction
  NdbOperation::LockMode m_lm;
  Res m_res;
};

static Spt sptlist[] = {{1, NdbOperation::LM_Read, Spt::Latest},
                        {1, NdbOperation::LM_Exclusive, Spt::Latest},
                        {1, NdbOperation::LM_CommittedRead, Spt::Latest},
                        {0, NdbOperation::LM_Read, Spt::Deadlock},
                        {0, NdbOperation::LM_Exclusive, Spt::Deadlock},
                        {0, NdbOperation::LM_CommittedRead, Spt::Committed}};
static uint sptcount = sizeof(sptlist) / sizeof(sptlist[0]);

static int savepointreadpk(const Par &par, Spt spt) {
  LL3("savepointreadpk");
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  const Set &set1 = set;
  Set set2(tab, set.m_rows);
  uint n = 0;
  for (uint i = 0; i < set.m_rows; i++) {
    set.lock();
    if (!set.compat(par, i, Row::OpREAD)) {
      LL4("savepointreadpk SKIP " << i << " " << set.getrow(i));
      set.unlock();
      continue;
    }
    set.unlock();
    CHK(set2.selrow(par, *set1.m_row[i]) == 0);
    uint err = par.m_catcherr | Con::ErrDeadlock;
    ExecType et = NoCommit;
    CHK(con.execute(et, err) == 0);
    if (err) {
      if (err & Con::ErrDeadlock) {
        CHK(spt.m_res == Spt::Deadlock);
        // all rows have same behaviour
        CHK(n == 0);
      }
      LL1("savepointreadpk stop on " << con.errname(err));
      break;
    }
    uint i2 = (uint)-1;
    CHK(set2.getkey(par, &i2) == 0 && i == i2);
    CHK(set2.putval(i, false) == 0);
    LL4("row " << set2.count() << " " << set2.getrow(i));
    n++;
  }
  bool dirty = (!spt.m_same && spt.m_lm == NdbOperation::LM_CommittedRead);
  if (spt.m_res != Spt::Deadlock)
    CHK(set1.verify(par, set2, false, dirty) == 0);
  return 0;
}

static int savepointreadhashindex(const Par &par, Spt spt) {
  if (spt.m_lm == NdbOperation::LM_CommittedRead && !spt.m_same) {
    LL1("skip hash index dirty read");
    return 0;
  }
  LL3("savepointreadhashindex");
  Con &con = par.con();
  const Tab &tab = par.tab();
  const ITab &itab = par.itab();
  Set &set = par.set();
  const Set &set1 = set;
  Set set2(tab, set.m_rows);
  uint n = 0;
  for (uint i = 0; i < set.m_rows; i++) {
    set.lock();
    if (!set.compat(par, i, Row::OpREAD)) {
      LL3("savepointreadhashindex SKIP " << i << " " << set.getrow(i));
      set.unlock();
      continue;
    }
    set.unlock();
    CHK(set2.selrow(par, itab, *set1.m_row[i]) == 0);
    uint err = par.m_catcherr | Con::ErrDeadlock;
    ExecType et = NoCommit;
    CHK(con.execute(et, err) == 0);
    if (err) {
      if (err & Con::ErrDeadlock) {
        CHK(spt.m_res == Spt::Deadlock);
        // all rows have same behaviour
        CHK(n == 0);
      }
      LL1("savepointreadhashindex stop on " << con.errname(err));
      break;
    }
    uint i2 = (uint)-1;
    CHK(set2.getkey(par, &i2) == 0 && i == i2);
    CHK(set2.putval(i, false) == 0);
    LL4("row " << set2.count() << " " << *set2.m_row[i]);
    n++;
  }
  bool dirty = (!spt.m_same && spt.m_lm == NdbOperation::LM_CommittedRead);
  if (spt.m_res != Spt::Deadlock)
    CHK(set1.verify(par, set2, false, dirty) == 0);
  return 0;
}

static int savepointscantable(const Par &par, Spt spt) {
  LL3("savepointscantable");
  Con &con = par.con();
  const Tab &tab = par.tab();
  const Set &set = par.set();
  const Set &set1 = set;      // not modifying current set
  Set set2(tab, set.m_rows);  // scan result
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.readTuples(par) == 0);
  set2.getval(par);  // getValue all columns
  CHK(con.executeScan() == 0);
  bool deadlock = false;
  uint n = 0;
  while (1) {
    int ret;
    uint err = par.m_catcherr | Con::ErrDeadlock;
    CHK((ret = con.nextScanResult(true, err)) == 0 || ret == 1);
    if (ret == 1) break;
    if (err) {
      if (err & Con::ErrDeadlock) {
        CHK(spt.m_res == Spt::Deadlock);
        // all rows have same behaviour
        CHK(n == 0);
        deadlock = true;
      }
      LL1("savepointscantable stop on " << con.errname(err));
      break;
    }
    CHK(spt.m_res != Spt::Deadlock);
    uint i = (uint)-1;
    CHK(set2.getkey(par, &i) == 0);
    CHK(set2.putval(i, false, n) == 0);
    LL4("row " << n << " key " << i << " " << set2.getrow(i));
    n++;
  }
  if (set1.m_rows > 0) {
    if (!deadlock)
      CHK(spt.m_res != Spt::Deadlock);
    else
      CHK(spt.m_res == Spt::Deadlock);
  }
  LL2("savepointscantable " << n << " rows");
  bool dirty = (!spt.m_same && spt.m_lm == NdbOperation::LM_CommittedRead);
  if (spt.m_res != Spt::Deadlock)
    CHK(set1.verify(par, set2, false, dirty) == 0);
  return 0;
}

static int savepointscanindex(const Par &par, Spt spt) {
  LL3("savepointscanindex");
  Con &con = par.con();
  const Tab &tab = par.tab();
  const ITab &itab = par.itab();
  const Set &set = par.set();
  const Set &set1 = set;
  Set set2(tab, set.m_rows);
  CHK(con.getNdbIndexScanOperation(itab, tab) == 0);
  CHK(con.readIndexTuples(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  bool deadlock = false;
  uint n = 0;
  while (1) {
    int ret;
    uint err = par.m_catcherr | Con::ErrDeadlock;
    CHK((ret = con.nextScanResult(true, err)) == 0 || ret == 1);
    if (ret == 1) break;
    if (err) {
      if (err & Con::ErrDeadlock) {
        CHK(spt.m_res == Spt::Deadlock);
        // all rows have same behaviour
        CHK(n == 0);
        deadlock = true;
      }
      LL1("savepointscanindex stop on " << con.errname(err));
      break;
    }
    CHK(spt.m_res != Spt::Deadlock);
    uint i = (uint)-1;
    CHK(set2.getkey(par, &i) == 0);
    CHK(set2.putval(i, par.m_dups, n) == 0);
    LL4("row " << n << " key " << i << " " << set2.getrow(i));
    n++;
  }
  if (set1.m_rows > 0) {
    if (!deadlock)
      CHK(spt.m_res != Spt::Deadlock);
    else
      CHK(spt.m_res == Spt::Deadlock);
  }
  LL2("savepointscanindex " << n << " rows");
  bool dirty = (!spt.m_same && spt.m_lm == NdbOperation::LM_CommittedRead);
  if (spt.m_res != Spt::Deadlock)
    CHK(set1.verify(par, set2, false, dirty) == 0);
  return 0;
}

typedef int (*SptFun)(const Par &, Spt);

static int savepointtest(const Par &par, Spt spt, SptFun fun) {
  Con &con = par.con();
  Par par2 = par;
  Con con2;
  if (!spt.m_same) {
    con2.connect(con);  // copy ndb reference
    par2.m_con = &con2;
    CHK(con2.startTransaction() == 0);
  }
  par2.m_lockmode = spt.m_lm;
  CHK((*fun)(par2, spt) == 0);
  if (!spt.m_same) {
    con2.closeTransaction();
  }
  return 0;
}

static int savepointtest(Par par, const char *op) {
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  LL2("savepointtest op=\"" << op << "\"");
  CHK(con.startTransaction() == 0);
  const char *p = op;
  char c;
  while ((c = *p++) != 0) {
    uint j;
    for (j = 0; j < par.m_rows; j++) {
      uint i = thrrow(par, j);
      if (c == 'c') {
        ExecType et = Commit;
        CHK(con.execute(et) == 0);
        set.lock();
        set.post(par, et);
        set.unlock();
        CHK(con.startTransaction() == 0);
      } else {
        set.lock();
        set.push(i);
        if (c == 'i') {
          set.calc(par, i);
          CHK(set.insrow(par, i) == 0);
        } else if (c == 'u') {
          set.copyval(i, tab.m_pkmask);
          set.calc(par, i, ~tab.m_pkmask);
          CHK(set.updrow(par, i) == 0);
        } else if (c == 'd') {
          set.copyval(i, tab.m_pkmask);
          CHK(set.delrow(par, i) == 0);
        } else {
          require(false);
        }
        set.unlock();
      }
    }
  }
  {
    ExecType et = NoCommit;
    CHK(con.execute(et) == 0);
    set.lock();
    set.post(par, et);
    set.unlock();
  }
  for (uint k = 0; k < sptcount; k++) {
    Spt spt = sptlist[k];
    LL2("spt lm=" << spt.m_lm << " same=" << spt.m_same);
    CHK(savepointtest(par, spt, &savepointreadpk) == 0);
    CHK(savepointtest(par, spt, &savepointscantable) == 0);
    for (uint i = 0; i < tab.m_itabs; i++) {
      if (tab.m_itab[i] == 0) continue;
      const ITab &itab = *tab.m_itab[i];
      par.m_itab = &itab;
      if (itab.m_type == ITab::OrderedIndex)
        CHK(savepointtest(par, spt, &savepointscanindex) == 0);
      else
        CHK(savepointtest(par, spt, &savepointreadhashindex) == 0);
      par.m_itab = 0;
    }
  }
  {
    ExecType et = Rollback;
    CHK(con.execute(et) == 0);
    set.lock();
    set.post(par, et);
    set.unlock();
  }
  con.closeTransaction();
  return 0;
}

static int savepointtest(Par par) {
  require(par.m_usedthreads == 1);
  const char *oplist[] = {// each based on previous and "c" not last
                          "i", "icu", "uuuuu", "d", "dciuuuuud", 0};
  int i;
  for (i = 0; oplist[i] != 0; i++) {
    CHK(savepointtest(par, oplist[i]) == 0);
  }
  return 0;
}

static int halloweentest(Par par, const ITab &itab) {
  LL2("halloweentest " << itab.m_name);
  Con &con = par.con();
  const Tab &tab = par.tab();
  Set &set = par.set();
  CHK(con.startTransaction() == 0);
  // insert 1 row
  uint i = 0;
  set.push(i);
  set.calc(par, i);
  CHK(set.insrow(par, i) == 0);
  CHK(con.execute(NoCommit) == 0);
  // scan via index until Set m_rows reached
  uint scancount = 0;
  bool stop = false;
  while (!stop) {
    par.m_lockmode =  // makes no difference
        scancount % 2 == 0 ? NdbOperation::LM_CommittedRead
                           : NdbOperation::LM_Read;
    Set set1(tab, set.m_rows);  // expected scan result
    Set set2(tab, set.m_rows);  // actual scan result
    BSet bset(tab, itab);
    calcscanbounds(par, itab, bset, set, set1);
    CHK(con.getNdbIndexScanOperation(itab, tab) == 0);
    CHK(con.readIndexTuples(par) == 0);
    CHK(bset.setbnd(par) == 0);
    set2.getval(par);
    CHK(con.executeScan() == 0);
    const uint savepoint = i;
    LL3("scancount=" << scancount << " savepoint=" << savepoint);
    uint n = 0;
    while (1) {
      int ret;
      CHK((ret = con.nextScanResult(true)) == 0 || ret == 1);
      if (ret == 1) break;
      uint k = (uint)-1;
      CHK(set2.getkey(par, &k) == 0);
      CHK(set2.putval(k, false, n) == 0);
      LL3("row=" << n << " key=" << k);
      CHK(k <= savepoint);
      if (++i == set.m_rows) {
        stop = true;
        break;
      }
      set.push(i);
      set.calc(par, i);
      CHK(set.insrow(par, i) == 0);
      CHK(con.execute(NoCommit) == 0);
      n++;
    }
    con.closeScan();
    LL3("scanrows=" << n);
    if (!stop) {
      CHK(set1.verify(par, set2, false) == 0);
    }
    scancount++;
  }
  CHK(con.execute(Commit) == 0);
  set.post(par, Commit);
  require(set.count() == set.m_rows);
  CHK(pkdelete(par) == 0);
  return 0;
}

static int halloweentest(Par par) {
  require(par.m_usedthreads == 1);
  const Tab &tab = par.tab();
  for (uint i = 0; i < tab.m_itabs; i++) {
    if (tab.m_itab[i] == 0) continue;
    const ITab &itab = *tab.m_itab[i];
    if (itab.m_type == ITab::OrderedIndex) CHK(halloweentest(par, itab) == 0);
  }
  return 0;
}

// threads

typedef int (*TFunc)(Par par);
enum TMode { ST = 1, MT = 2 };

extern "C" {
static void *runthread(void *arg);
}

struct Thr {
  const char *m_name;
  enum State { Wait, Start, Stop, Exit };
  State m_state;
  Par m_par;
  my_thread_t m_id;
  NdbThread *m_thread;
  NdbMutex *m_mutex;
  NdbCondition *m_cond;
  TFunc m_func;
  int m_ret;
  void *m_status;
  char m_tmp[20];  // used for debug msg prefix
  Thr(const Par &par, uint n);
  ~Thr();
  int run();
  void start();
  void stop();
  void exit();
  //
  void lock() { NdbMutex_Lock(m_mutex); }
  void unlock() { NdbMutex_Unlock(m_mutex); }
  void wait() { NdbCondition_Wait(m_cond, m_mutex); }
  void signal() { NdbCondition_Signal(m_cond); }
  void join() { NdbThread_WaitFor(m_thread, &m_status); }
};

Thr::Thr(const Par &par, uint n)
    : m_name(0),
      m_state(Wait),
      m_par(par),
      m_thread(0),
      m_mutex(0),
      m_cond(0),
      m_func(0),
      m_ret(0),
      m_status(0) {
  m_par.m_no = n;
  char buf[10];
  sprintf(buf, "thr%03u", par.m_no);
  m_name = strcpy(new char[10], buf);
  // mutex
  m_mutex = NdbMutex_Create();
  m_cond = NdbCondition_Create();
  require(m_mutex != 0 && m_cond != 0);
  // run
  const uint stacksize = 256 * 1024;
  const NDB_THREAD_PRIO prio = NDB_THREAD_PRIO_LOW;
  m_thread =
      NdbThread_Create(runthread, (void **)this, stacksize, m_name, prio);
}

Thr::~Thr() {
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
  delete[] m_name;
}

static void *runthread(void *arg) {
  Thr &thr = *(Thr *)arg;
  thr.m_id = my_thread_self();
  if (thr.run() < 0) {
    LL1("exit on error");
  } else {
    LL4("exit ok");
  }
  return 0;
}

int Thr::run() {
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
    require(m_state == Start);
    m_ret = (*m_func)(m_par);
    m_state = Stop;
    LL4("stop");
    signal();
    unlock();
    if (m_ret == -1) {
      if (m_par.m_cont)
        LL1("continue running due to -cont");
      else
        return -1;
    }
  }
  con.disconnect();
  return 0;
}

void Thr::start() {
  lock();
  m_state = Start;
  signal();
  unlock();
}

void Thr::stop() {
  lock();
  while (m_state != Stop) wait();
  m_state = Wait;
  unlock();
}

void Thr::exit() {
  lock();
  m_state = Exit;
  signal();
  unlock();
}

// test run

static Thr **g_thrlist = 0;

static Thr *getthr() {
  if (g_thrlist != 0) {
    my_thread_t id = my_thread_self();
    for (uint n = 0; n < g_opt.m_threads; n++) {
      if (g_thrlist[n] != 0) {
        Thr &thr = *g_thrlist[n];
        if (my_thread_equal(thr.m_id, id)) return &thr;
      }
    }
  }
  return 0;
}

// for debug messages (par.m_no not available)
static const char *getthrprefix() {
  Thr *thrp = getthr();
  if (thrp != 0) {
    Thr &thr = *thrp;
    uint n = thr.m_par.m_no;
    uint m = g_opt.m_threads < 10 ? 1 : g_opt.m_threads < 100 ? 2 : 3;
    sprintf(thr.m_tmp, "[%0*u] ", m, n);
    return thr.m_tmp;
  }
  return "";
}

static int runstep(Par par, const char *fname, TFunc func, uint mode) {
  LL2("step: " << fname);
  const int threads = (mode & ST ? 1 : par.m_usedthreads);
  int n;
  for (n = 0; n < threads; n++) {
    LL4("start " << n);
    Thr &thr = *g_thrlist[n];
    Par oldpar = thr.m_par;
    // update parameters
    thr.m_par = par;
    thr.m_par.m_no = oldpar.m_no;
    thr.m_par.m_con = oldpar.m_con;
    thr.m_func = func;
    thr.start();
  }
  uint errs = 0;
  for (n = threads - 1; n >= 0; n--) {
    LL4("stop " << n);
    Thr &thr = *g_thrlist[n];
    thr.stop();
    if (thr.m_ret != 0) errs++;
  }
  CHK(errs == 0);
  return 0;
}

#define RUNSTEP(par, func, mode) CHK(runstep(par, #func, func, mode) == 0)

#define SUBLOOP(par)                                                          \
  "sloop: " << par.m_lno << "/" << par.m_currcase << "/" << par.m_tab->m_name \
            << "/" << par.m_slno

static int tbuild(Par par) {
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    if (par.m_slno % 3 == 0) {
      RUNSTEP(par, createindex, ST);
      RUNSTEP(par, invalidateindex, MT);
      RUNSTEP(par, pkinsert, MT);
      RUNSTEP(par, pkupdate, MT);
    } else if (par.m_slno % 3 == 1) {
      RUNSTEP(par, pkinsert, MT);
      RUNSTEP(par, createindex, ST);
      RUNSTEP(par, invalidateindex, MT);
      RUNSTEP(par, pkupdate, MT);
    } else {
      RUNSTEP(par, pkinsert, MT);
      RUNSTEP(par, pkupdate, MT);
      RUNSTEP(par, createindex, ST);
      RUNSTEP(par, invalidateindex, MT);
    }
    RUNSTEP(par, readverifyfull, MT);
    // leave last one
    if (par.m_slno + 1 < par.m_sloop) {
      RUNSTEP(par, pkdelete, MT);
      RUNSTEP(par, readverifyfull, MT);
      RUNSTEP(par, dropindex, ST);
    }
  }
  return 0;
}

static int tindexscan(Par par) {
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, readverifyfull, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    RUNSTEP(par, readverifyindex, MT);
  }
  return 0;
}

static int tpkops(Par par) {
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    RUNSTEP(par, pkops, MT);
    LL2("rows=" << par.set().count());
    RUNSTEP(par, readverifyfull, MT);
  }
  return 0;
}

static int tpkopsread(Par par) {
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, readverifyfull, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    RUNSTEP(par, pkupdatescanread, MT);
    RUNSTEP(par, readverifyfull, MT);
  }
  RUNSTEP(par, pkdelete, MT);
  RUNSTEP(par, readverifyfull, MT);
  return 0;
}

static int tmixedops(Par par) {
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, readverifyfull, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    RUNSTEP(par, mixedoperations, MT);
    RUNSTEP(par, readverifyfull, MT);
  }
  return 0;
}

static int tbusybuild(Par par) {
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    RUNSTEP(par, pkupdateindexbuild, MT);
    RUNSTEP(par, invalidateindex, MT);
    RUNSTEP(par, readverifyfull, MT);
    RUNSTEP(par, dropindex, ST);
  }
  return 0;
}

static int trollback(Par par) {
  par.m_abortpct = 50;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, readverifyfull, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    RUNSTEP(par, mixedoperations, MT);
    RUNSTEP(par, readverifyfull, MT);
  }
  return 0;
}

static int tparupdate(Par par) {
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, readverifyfull, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    RUNSTEP(par, parallelorderedupdate, MT);
    RUNSTEP(par, readverifyfull, MT);
  }
  return 0;
}

static int tsavepoint(Par par) {
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    RUNSTEP(par, savepointtest, MT);
    RUNSTEP(par, readverifyfull, MT);
  }
  return 0;
}

static int thalloween(Par par) {
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
    RUNSTEP(par, halloweentest, MT);
  }
  return 0;
}

static int ttimebuild(Par par) {
  Tmr t1;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
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

static int ttimemaint(Par par) {
  Tmr t1, t2;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
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

static int ttimescan(Par par) {
  if (par.tab().m_itab[0] == 0) {
    LL1("ttimescan - no index 0, skipped");
    return 0;
  }
  Tmr t1, t2;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
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

static int ttimepkread(Par par) {
  if (par.tab().m_itab[0] == 0) {
    LL1("ttimescan - no index 0, skipped");
    return 0;
  }
  Tmr t1, t2;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (par.m_slno = 0; par.m_slno < par.m_sloop; par.m_slno++) {
    LL1(SUBLOOP(par));
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

static int tdrop(Par par) {
  RUNSTEP(par, droptable, ST);
  return 0;
}

struct TCase {
  const char *m_name;
  TFunc m_func;
  const char *m_desc;
  TCase(const char *name, TFunc func, const char *desc)
      : m_name(name), m_func(func), m_desc(desc) {}
};

static const TCase tcaselist[] = {
    TCase("a", tbuild, "index build"),
    TCase("b", tindexscan, "index scans"),
    TCase("c", tpkops, "pk operations"),
    TCase("d", tpkopsread, "pk operations and scan reads"),
    TCase("e", tmixedops, "pk operations and scan operations"),
    TCase("f", tbusybuild, "pk operations and index build"),
    TCase("g", trollback, "operations with random rollbacks"),
    TCase("h", tparupdate, "parallel ordered update bug#20446"),
    TCase("i", tsavepoint, "savepoint test locking bug#31477"),
    TCase("j", thalloween, "savepoint test halloween problem"),
    TCase("t", ttimebuild, "time index build"),
    TCase("u", ttimemaint, "time index maintenance"),
    TCase("v", ttimescan, "time full scan table vs index on pk"),
    TCase("w", ttimepkread, "time pk read table vs index on pk"),
    TCase("z", tdrop, "drop test tables")};

static const uint tcasecount = sizeof(tcaselist) / sizeof(tcaselist[0]);

static void printcases() {
  ndbout << "test cases:" << endl;
  for (uint i = 0; i < tcasecount; i++) {
    const TCase &tcase = tcaselist[i];
    ndbout << "  " << tcase.m_name << " - " << tcase.m_desc << endl;
  }
}

static void printtables() {
  Par par(g_opt);
  makebuiltintables(par);
  ndbout << "tables and indexes (x=ordered z=hash x0=on pk):" << endl;
  for (uint j = 0; j < tabcount; j++) {
    if (tablist[j] == 0) continue;
    const Tab &tab = *tablist[j];
    const char *tname = tab.m_name;
    ndbout << "  " << tname;
    for (uint i = 0; i < tab.m_itabs; i++) {
      if (tab.m_itab[i] == 0) continue;
      const ITab &itab = *tab.m_itab[i];
      const char *iname = itab.m_name;
      if (strncmp(tname, iname, strlen(tname)) == 0) iname += strlen(tname);
      ndbout << " " << iname;
      ndbout << "(";
      for (uint k = 0; k < itab.m_icols; k++) {
        if (k != 0) ndbout << ",";
        const ICol &icol = *itab.m_icol[k];
        const Col &col = icol.m_col;
        ndbout << col.m_name;
      }
      ndbout << ")";
    }
    ndbout << endl;
  }
}

static bool setcasepar(Par &par) {
  Opt d;
  const char *c = par.m_currcase;
  switch (c[0]) {
    case 'i': {
      if (par.m_usedthreads > 1) {
        par.m_usedthreads = 1;
        LL1("case " << c << " reduce threads to " << par.m_usedthreads);
      }
      const uint rows = 100;
      if (par.m_rows > rows) {
        par.m_rows = rows;
        LL1("case " << c << " reduce rows to " << rows);
      }
    } break;
    case 'j': {
      if (par.m_usedthreads > 1) {
        par.m_usedthreads = 1;
        LL1("case " << c << " reduce threads to " << par.m_usedthreads);
      }
    } break;
    default:
      break;
  }
  return true;
}

static int runtest(Par par) {
  int totret = 0;
  if (par.m_seed == -1) {
    // good enough for daily run
    const int seed = NdbHost_GetProcessId();
    LL0("random seed: " << seed);
    srandom(seed);
  } else if (par.m_seed != 0) {
    LL0("random seed: " << par.m_seed);
    srandom(par.m_seed);
  } else {
    LL0("random seed: loop number");
  }
  // cs
  require(par.m_csname != 0);
  if (strcmp(par.m_csname, "random") != 0) {
    CHARSET_INFO *cs;
    CHK((cs = get_charset_by_name(par.m_csname, MYF(0))) != 0 ||
        (cs = get_charset_by_csname(par.m_csname, MY_CS_PRIMARY, MYF(0))) != 0);
    par.m_cs = cs;
  }
  // con
  Con con;
  CHK(con.connect() == 0);
  par.m_con = &con;
  par.m_catcherr |= Con::ErrNospace;
  par.m_catcherr |= Con::ErrLogspace;
  // threads
  g_thrlist = new Thr *[par.m_threads];
  uint n;
  for (n = 0; n < par.m_threads; n++) {
    g_thrlist[n] = 0;
  }
  for (n = 0; n < par.m_threads; n++) {
    g_thrlist[n] = new Thr(par, n);
    Thr &thr = *g_thrlist[n];
    require(thr.m_thread != 0);
  }
  for (par.m_lno = 0; par.m_loop == 0 || par.m_lno < par.m_loop; par.m_lno++) {
    LL1("loop: " << par.m_lno);
    if (par.m_seed == 0) {
      LL1("random seed: " << par.m_lno);
      srandom(par.m_lno);
    }
    for (uint i = 0; i < tcasecount; i++) {
      const TCase &tcase = tcaselist[i];
      if ((par.m_case != 0 && strchr(par.m_case, tcase.m_name[0]) == 0) ||
          (par.m_skip != 0 && strchr(par.m_skip, tcase.m_name[0]) != 0)) {
        continue;
      }
      sprintf(par.m_currcase, "%c", tcase.m_name[0]);
      par.m_usedthreads = par.m_threads;
      if (!setcasepar(par)) {
        LL1("case " << tcase.m_name << " cannot run with given options");
        continue;
      }
      par.m_totrows = par.m_usedthreads * par.m_rows;
      makebuiltintables(par);
      LL1("case: " << par.m_lno << "/" << tcase.m_name << " - "
                   << tcase.m_desc);
      for (uint j = 0; j < tabcount; j++) {
        if (tablist[j] == 0) continue;
        const Tab &tab = *tablist[j];
        par.m_tab = &tab;
        par.m_set = new Set(tab, par.m_totrows);
        LL1("table: " << par.m_lno << "/" << tcase.m_name << "/" << tab.m_name);
        int ret = tcase.m_func(par);
        delete par.m_set;
        par.m_set = 0;
        if (ret == -1) {
          if (!par.m_cont) return -1;
          totret = -1;
          LL1("continue to next case due to -cont");
          break;
        }
      }
    }
  }
  for (n = 0; n < par.m_threads; n++) {
    Thr &thr = *g_thrlist[n];
    thr.exit();
  }
  for (n = 0; n < par.m_threads; n++) {
    Thr &thr = *g_thrlist[n];
    thr.join();
    delete &thr;
  }
  delete[] g_thrlist;
  g_thrlist = 0;
  con.disconnect();
  return totret;
}

static const char *g_progname = "testOIBasic";

int main(int argc, char **argv) {
  initcslist();
  ndb_init();
  uint i;
  ndbout << g_progname;
  for (i = 1; i < (uint)argc; i++) ndbout << " " << argv[i];
  ndbout << endl;
  ndbout_mutex = NdbMutex_Create();
  while (++argv, --argc > 0) {
    const char *arg = argv[0];
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
        const char *p = argv[0];
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
    if (strcmp(arg, "-cont") == 0) {
      g_opt.m_cont = true;
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
    if (strcmp(arg, "-mrrmaxrng") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_mrrmaxrng = atoi(argv[0]);
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
    if (strcmp(arg, "-pctmrr") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_pctmrr = atoi(argv[0]);
        continue;
      }
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
    if (strcmp(arg, "-scanbatch") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_scanbatch = atoi(argv[0]);
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
    if (strcmp(arg, "-skip") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_skip = strdup(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-sloop") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_sloop = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-ssloop") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_ssloop = atoi(argv[0]);
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
        if (1 <= g_opt.m_threads) continue;
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
    g_ncc->configure_tls(opt_tls_search_path, opt_mgm_tls);
    if (g_ncc->connect(30) != 0 || runtest(par) < 0) goto failed;
    delete g_ncc;
    g_ncc = 0;
  }
  // ok
  if (tablist) {
    for (uint i = 0; i < tabcount; i++) {
      delete tablist[i];
    }
    delete[] tablist;
  }
  for (uint i = 0; i < maxcsnumber; i++) {
    delete cslist[i];
  }
  ndb_end(0);
  return NDBT_ProgramExit(NDBT_OK);
failed:
  return NDBT_ProgramExit(NDBT_FAILED);
usage:
  ndbout << " (use -h for help)" << endl;
wrongargs:
  return NDBT_ProgramExit(NDBT_WRONGARGS);
}

// vim: set sw=2 et:
