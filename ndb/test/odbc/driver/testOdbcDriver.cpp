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
 * testOdbcDriver
 *
 * Test of ODBC and SQL using a fixed set of tables.
 */

#include <ndb_global.h>
#undef test
#include <ndb_version.h>
#include <kernel/ndb_limits.h>
#include <Bitmask.hpp>
#include <kernel/AttributeList.hpp>
#ifdef ndbODBC
#include <NdbApi.hpp>
#endif
#include <sqlext.h>

#undef BOOL

#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <NdbTick.h>
#include <NdbSleep.h>

#ifdef ndbODBC
#include <NdbTest.hpp>
#else
#define NDBT_OK			0
#define NDBT_FAILED		1
#define NDBT_WRONGARGS		2
static int
NDBT_ProgramExit(int rcode)
{
    const char* rtext = "Unknown";
    switch (rcode) {
    case NDBT_OK:
	rtext = "OK";
	break;
    case NDBT_FAILED:
	rtext = "Failed";
	break;
    case NDBT_WRONGARGS:
	rtext = "Wrong arguments";
	break;
    };
    ndbout_c("\nNDBT_ProgramExit: %d - %s\n", rcode, rtext);
    return rcode;
}
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define arraySize(x)	(sizeof(x)/sizeof(x[0]))

#define SQL_ATTR_NDB_TUPLES_FETCHED	66601

// options

#define MAX_THR		128		// max threads

struct Opt {
    const char* m_name[100];
    unsigned m_namecnt;
    bool m_core;
    unsigned m_depth;
    const char* m_dsn;
    unsigned m_errs;
    const char* m_fragtype;
    unsigned m_frob;
    const char* m_home;
    unsigned m_loop;
    bool m_nogetd;
    bool m_noputd;
    bool m_nosort;
    unsigned m_scale;
    bool m_serial;
    const char* m_skip[100];
    unsigned m_skipcnt;
    unsigned m_subloop;
    const char* m_table;
    unsigned m_threads;
    unsigned m_trace;
    unsigned m_v;
    Opt() :
	m_namecnt(0),
	m_core(false),
	m_depth(5),
	m_dsn("NDB"),
	m_errs(0),
        m_fragtype(0),
        m_frob(0),
	m_home(0),
	m_loop(1),
	m_nogetd(false),
	m_noputd(false),
	m_nosort(false),
	m_scale(100),
	m_serial(false),
	m_skipcnt(0),
	m_subloop(1),
	m_table(0),
	m_threads(1),
	m_trace(0),
	m_v(1) {
	for (unsigned i = 0; i < arraySize(m_name); i++)
	    m_name[i] = 0;
	for (unsigned i = 0; i < arraySize(m_skip); i++)
	    m_skip[i] = 0;
    }
};

static Opt opt;

static void listCases();
static void listTables();
static void printusage()
{
    Opt d;
    ndbout
	<< "usage: testOdbcDriver [options]" << endl
	<< "-case name  run only named tests (substring match - can be repeated)" << endl
	<< "-core       dump core on failure" << endl
	<< "-depth N    join depth - default " << d.m_depth << endl
	<< "-dsn string data source name - default " << d.m_dsn << endl
	<< "-errs N     allow N errors before quitting - default " << d.m_errs << endl
        << "-fragtype t fragment type single/small/medium/large" << d.m_errs << endl
        << "-frob X     case-dependent tweak (number)" << endl
	<< "-home dir   set NDB_HOME (contains Ndb.cfg)" << endl
	<< "-loop N     loop N times (0 = forever) - default " << d.m_loop << endl
	<< "-nogetd     do not use SQLGetData - default " << d.m_nogetd << endl
	<< "-noputd     do not use SQLPutData - default " << d.m_noputd << endl
	<< "-nosort     no order-by in verify scan (checks non-Pk values only)" << endl
	<< "-scale N    row count etc - default " << d.m_scale << endl
	<< "-serial     run multi-threaded test cases one at a time" << endl
	<< "-skip name  skip named tests (substring match - can be repeated)" << endl
	<< "-subloop N  loop count per case (same threads) - default " << d.m_subloop << endl
	<< "-table T    do only table T (table name on built-in list)" << endl
	<< "-threads N  number of threads (max " << MAX_THR << ") - default " << d.m_threads << endl
	<< "-trace N    trace in NDB ODBC driver - default " << d.m_trace << endl
	<< "-v N        verbosity - default " << d.m_v << endl
	;
    listCases();
    listTables();
}

static void
fatal(const char* fmt, ...)
{
    va_list ap;
    char buf[200];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ndbout << buf << endl;
    if (opt.m_errs != 0) {
	opt.m_errs--;
	return;
    }
    if (opt.m_core)
	abort();
    NDBT_ProgramExit(NDBT_FAILED);
    exit(1);
}

static void
cleanprint(const char* s, unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
	char b[10];
	if (0x20 < s[i] && s[i] <= 0x7e)
	    sprintf(b, "%c", s[i]);
	else
	    sprintf(b, "\\%02x", (unsigned)s[i]);
	ndbout << b;
    }
}

// global mutex
static NdbMutex my_mutex = NDB_MUTEX_INITIALIZER;
static void lock_mutex() { NdbMutex_Lock(&my_mutex); }
static void unlock_mutex() { NdbMutex_Unlock(&my_mutex); }

// semaphore zeroed before each call to a test routine
static unsigned my_sema = 0;

// print mutex
static NdbMutex out_mutex = NDB_MUTEX_INITIALIZER;
static NdbOut& lock(NdbOut& out) { NdbMutex_Lock(&out_mutex); return out; }
static NdbOut& unlock(NdbOut& out) { NdbMutex_Unlock(&out_mutex); return out; }

static unsigned
urandom(unsigned n)
{
    assert(n != 0);
    unsigned i = random();
    return i % n;
}

// test cases

struct Test;

struct Case {
    enum Mode {
	Single = 1,		// single thread
	Serial = 2,		// all threads but one at a time
	Thread = 3		// all threads in parallel
    };
    const char* m_name;
    void (*m_func)(Test& test);
    Mode m_mode;
    unsigned m_stuff;
    const char* m_desc;
    Case(const char* name, void (*func)(Test& test), Mode mode, unsigned stuff, const char* desc) :
	m_name(name),
	m_func(func),
	m_mode(mode),
	m_stuff(stuff),
	m_desc(desc) {
    }
    const char* modename() const {
	const char* s = "?";
	if (m_mode == Case::Single)
	    return "Single";
	if (m_mode == Case::Serial)
	    return "Serial";
	if (m_mode == Case::Thread)
	    return "Thread";
	return "?";
    }
    bool matchcase() const {
	if (opt.m_namecnt == 0)
	    return ! skipcase();
	for (unsigned i = 0; i < opt.m_namecnt; i++) {
	    if (strstr(m_name, opt.m_name[i]) != 0)
		return ! skipcase();
	}
	return false;
    }
private:
    bool skipcase() const {
	for (unsigned i = 0; i < opt.m_skipcnt; i++) {
	    if (strstr(m_name, opt.m_skip[i]) != 0)
		return true;
	}
	return false;
    }
};

// calculate values

struct Calc {
    enum { m_mul = 1000000 };
    unsigned m_no;
    unsigned m_base;
    unsigned m_salt;		// modifies non-PK values
    bool m_const;		// base non-PK values on PK of row 0
    Calc(unsigned no) :
	m_no(no),
	m_salt(0),
	m_const(false) {
	m_base = m_no * m_mul;
    }
    void calcPk(unsigned rownum, char* v, unsigned n) const {
	char b[10];
	sprintf(b, "%08x", m_base + rownum);
	for (unsigned i = 0; i < n; i++) {
	    char c = i < n - 1 ? b[i % 8] : 0;
	    v[i] = c;
	}
    }
    void calcPk(unsigned rownum, long* v) const {
	*v = m_base + rownum;
    }
    void hashPk(unsigned* hash, const char* v, unsigned n) const {
	for (unsigned i = 0; i < n; i++) {
	    *hash ^= (v[i] << i);
	}
    }
    void hashPk(unsigned* hash, long v) const {
	*hash ^= v;
    }
    void calcNk(unsigned hash, char* v, unsigned n, SQLINTEGER* ind, bool null) const {
	unsigned m = hash % n;
	for (unsigned i = 0; i < n; i++) {
	    char c = i < m ? 'a' + (hash + i) % ('z' - 'a' + 1) : i < n - 1 ? ' ' : 0;
	    v[i] = c;
	}
	*ind = null && hash % 9 == 0 ? SQL_NULL_DATA : SQL_NTS;
    }
    void calcNk(unsigned hash, long* v, SQLINTEGER* ind, bool null) const {
	*v = long(hash);
	*ind = null && hash % 7 == 0 ? SQL_NULL_DATA : 0;
    }
    void calcNk(unsigned hash, double* v, SQLINTEGER* ind, bool null) const {
	*v = long(hash) / 1000.0;
	*ind = null && hash % 5 == 0 ? SQL_NULL_DATA : 0;
    }
    bool verify(const char* v1, SQLINTEGER ind1, const char* v2, SQLINTEGER ind2, unsigned n) {
	if (ind1 == SQL_NULL_DATA && ind2 == SQL_NULL_DATA)
	    return true;
	if (ind1 != SQL_NULL_DATA && ind2 != SQL_NULL_DATA)
	    if (memcmp(v1, v2, n) == 0)
		return true;
	if (ind1 == SQL_NULL_DATA)
	    v1 = "NULL";
	if (ind2 == SQL_NULL_DATA)
	    v2 = "NULL";
	ndbout << "verify failed: got ";
	if (ind1 == SQL_NULL_DATA)
	    ndbout << "NULL";
	else
	    cleanprint(v1, n);
	ndbout << " != ";
	if (ind2 == SQL_NULL_DATA)
	    ndbout << "NULL";
	else
	    cleanprint(v2, n);
	ndbout << endl;
	return false;
    }
    bool verify(long v1, SQLINTEGER ind1, long v2, SQLINTEGER ind2) {
	char buf1[40], buf2[40];
	if (ind1 == SQL_NULL_DATA && ind2 == SQL_NULL_DATA)
	    return true;
	if (ind1 != SQL_NULL_DATA && ind2 != SQL_NULL_DATA)
	    if (v1 == v2)
		return true;
	if (ind1 == SQL_NULL_DATA)
	    strcpy(buf1, "NULL");
	else
	    sprintf(buf1, "%ld", v1);
	if (ind2 == SQL_NULL_DATA)
	    strcpy(buf2, "NULL");
	else
	    sprintf(buf2, "%ld", v2);
	ndbout << "verify failed: got " << buf1 << " != " << buf2 << endl;
	return false;
    }
    bool verify(double v1, SQLINTEGER ind1, double v2, SQLINTEGER ind2) {
	char buf1[40], buf2[40];
	if (ind1 == SQL_NULL_DATA && ind2 == SQL_NULL_DATA)
	    return true;
	if (ind1 != SQL_NULL_DATA && ind2 != SQL_NULL_DATA)
	    if (fabs(v1 - v2) < 1)	// XXX
		return true;
	if (ind1 == SQL_NULL_DATA)
	    strcpy(buf1, "NULL");
	else
	    sprintf(buf1, "%.10f", v1);
	if (ind2 == SQL_NULL_DATA)
	    strcpy(buf2, "NULL");
	else
	    sprintf(buf2, "%.10f", v2);
	ndbout << "verify failed: got " << buf1 << " != " << buf2 << endl;
	return false;
    }
};

#if defined(NDB_SOLARIS) || defined(NDB_LINUX) || defined(NDB_MACOSX)
#define HAVE_SBRK
#else
#undef HAVE_SBRK
#endif

struct Timer {
    Timer() :
	m_cnt(0),
	m_calls(0),
	m_on(0),
	m_msec(0)
#ifndef NDB_WIN32
	,
	m_brk(0),
	m_incr(0)
#endif
	{
    }
    void timerOn() {
	m_cnt = 0;
	m_calls = 0;
	m_on = NdbTick_CurrentMillisecond();
#ifdef HAVE_SBRK
	m_brk = (int)sbrk(0);
#endif
    }
    void timerOff() {
	m_msec = NdbTick_CurrentMillisecond() - m_on;
	if (m_msec <= 0)
	    m_msec = 1;
#ifdef HAVE_SBRK
	m_incr = (int)sbrk(0) - m_brk;
	if (m_incr < 0)
	    m_incr = 0;
#endif
    }
    void timerCnt(unsigned cnt) {
	m_cnt += cnt;
    }
    void timerCnt(const Timer& timer) {
	m_cnt += timer.m_cnt;
	m_calls += timer.m_calls;
    }
    friend NdbOut& operator<<(NdbOut& out, const Timer& timer) {
	out << timer.m_cnt << " ( " << 1000 * timer.m_cnt / timer.m_msec << "/sec )";
#ifdef HAVE_SBRK
	out << " - " << timer.m_incr << " sbrk";
	if (opt.m_namecnt != 0) {	// per case meaningless if many cases
	    if (timer.m_cnt > 0)
		out << " ( " << timer.m_incr / timer.m_cnt << "/cnt )";
	}
#endif
	out << " - " << timer.m_calls << " calls";
	return out;
    }
protected:
    unsigned m_cnt;	// count rows or whatever
    unsigned m_calls;	// count ODBC function calls
    NDB_TICKS m_on;
    unsigned m_msec;
#ifdef HAVE_SBRK
    int m_brk;
    int m_incr;
#endif
};

#define MAX_MESSAGE	500
#define MAX_DIAG	20

struct Diag {
    char m_state[5+1];
    SQLINTEGER m_native;
    char m_message[MAX_MESSAGE];
    unsigned m_flag;	// temp use
    Diag() {
	strcpy(m_state, "00000");
	m_native = 0;
	memset(m_message, 0, sizeof(m_message));
	m_flag = 0;
    }
    const char* text() {
	snprintf(m_buf, sizeof(m_buf), "%s %d '%s'", m_state, (int)m_native, m_message);
	return m_buf;
    }
    void getDiag(SQLSMALLINT type, SQLHANDLE handle, unsigned k, unsigned count) {
	int ret;
	SQLSMALLINT length = -1;
	memset(m_message, 0, MAX_MESSAGE);
	ret = SQLGetDiagRec(type, handle, k, (SQLCHAR*)m_state, &m_native, (SQLCHAR*)m_message, MAX_MESSAGE, &length);
	if (k <= count && ret != SQL_SUCCESS)
	    fatal("SQLGetDiagRec %d of %d: return %d != SQL_SUCCESS", k, count, (int)ret);
	if (k <= count && strlen(m_message) != length)
	    fatal("SQLGetDiagRec %d of %d: message length %d != %d", k, count, strlen(m_message), length);
	if (k > count && ret != SQL_NO_DATA)
	    fatal("SQLGetDiagRec %d of %d: return %d != SQL_NO_DATA", k, count, (int)ret);
	m_flag = 0;
    }
private:
    char m_buf[MAX_MESSAGE];
};

struct Diags {
    Diag m_diag[MAX_DIAG];
    SQLINTEGER m_diagCount;
    SQLINTEGER m_rowCount;
    SQLINTEGER m_functionCode;
    void getDiags(SQLSMALLINT type, SQLHANDLE handle) {
	int ret;
	m_diagCount = -1;
	ret = SQLGetDiagField(type, handle, 0, SQL_DIAG_NUMBER, &m_diagCount, SQL_IS_INTEGER, 0);
	if (ret == SQL_INVALID_HANDLE)
	    return;
	if (ret != SQL_SUCCESS)
	    fatal("SQLGetDiagField: return %d != SQL_SUCCESS", (int)ret);
	if (m_diagCount < 0 || m_diagCount > MAX_DIAG)
	    fatal("SQLGetDiagField: count %d", (int)m_diagCount);
	for (unsigned k = 0; k < MAX_DIAG; k++) {
	    m_diag[k].getDiag(type, handle, k + 1, m_diagCount);
	    if (k == m_diagCount + 1)
		break;
	}
	m_rowCount = -1;
	m_functionCode = SQL_DIAG_UNKNOWN_STATEMENT;
	if (type == SQL_HANDLE_STMT) {
	    ret = SQLGetDiagField(type, handle, 0, SQL_DIAG_ROW_COUNT, &m_rowCount, SQL_IS_INTEGER, 0);
#ifndef iODBC
	    if (ret != SQL_SUCCESS)
		fatal("SQLGetDiagField: return %d != SQL_SUCCESS", (int)ret);
#endif
	    ret = SQLGetDiagField(type, handle, 0, SQL_DIAG_DYNAMIC_FUNCTION_CODE, &m_functionCode, SQL_IS_INTEGER, 0);
	}
    }
    void showDiags() {
	for (unsigned k = 0; 0 <= m_diagCount && k < m_diagCount; k++) {
	    Diag& diag = m_diag[k];
	    ndbout << "diag " << k + 1;
	    ndbout << (diag.m_flag ? " [*]" : " [ ]");
	    ndbout << " " << diag.text() << endl;
	    if (k > 10)
		abort();
	}
    }
};

struct Exp {
    int m_ret;
    const char* m_state;
    SQLINTEGER m_native;
    Exp() : m_ret(SQL_SUCCESS), m_state(""), m_native(0) {}
    Exp(int ret, const char* state) : m_ret(ret), m_state(state) {}
};

struct Test : Calc, Timer, Diags {
    Test(unsigned no, unsigned loop) :
	Calc(no),
	m_loop(loop),
	m_stuff(0),
	m_perf(false),
	ccp(0) {
	exp(SQL_SUCCESS, 0, 0, true);
    }
    unsigned m_loop;			// current loop
    Exp m_expList[20];			// expected results
    unsigned m_expCount;
    int m_ret;				// actual return code
    int m_stuff;			// the stuff of abuse
    bool m_perf;			// check no diags on success
    const Case* ccp;			// current case
    void exp(int ret, const char* state, SQLINTEGER native, bool reset) {
	if (reset)
	    m_expCount = 0;
	unsigned i = m_expCount++;
	assert(i < arraySize(m_expList) - 1);
	m_expList[i].m_ret = ret;
	m_expList[i].m_state = state == 0 ? "" : state;
	m_expList[i].m_native = native;
    }
    void runCase(const Case& cc) {
	ccp = &cc;
	if (opt.m_v >= 3)
	    ndbout << cc.m_name << ": start" << endl;
	m_rowCount = -1;
	NDB_TICKS m_ms1 = NdbTick_CurrentMillisecond();
	m_salt = m_loop | (16 << cc.m_stuff);
	m_const = cc.m_stuff == 0;
	m_stuff = cc.m_stuff;
	(*cc.m_func)(*this);
	NDB_TICKS m_ms2 = NdbTick_CurrentMillisecond();
    }
    void run(SQLSMALLINT type, SQLHANDLE handle, int line, int ret) {
	m_calls++;
	m_ret = ret;
	if (m_perf && (m_ret == SQL_SUCCESS))
	    return;
	m_diagCount = 0;
	if (handle != SQL_NULL_HANDLE)
	    getDiags(type, handle);
	if (m_diagCount <= 0 && (ret != SQL_SUCCESS && ret != SQL_INVALID_HANDLE && ret != SQL_NEED_DATA && ret != SQL_NO_DATA)) {
	    fatal("%s: thr %d line %d: ret=%d but no diag records", ccp->m_name, m_no, line, ret);
	}
	for (unsigned k = 0; 0 <= m_diagCount && k < m_diagCount; k++) {
	    Diag& diag = m_diag[k];
	    bool match = false;
	    for (unsigned i = 0; i < m_expCount; i++) {
		if (strcmp(diag.m_state, m_expList[i].m_state) == 0 && (diag.m_native % 10000 == m_expList[i].m_native % 10000 || m_expList[i].m_native == -1)) {
		    match = true;
		    diag.m_flag = 0;
		    continue;
		}
		diag.m_flag = 1;	// mark unexpected
	    }
	    if (! match) {
		showDiags();
		fatal("%s: thr %d line %d: unexpected diag [*] ret=%d cnt=%d", ccp->m_name, m_no, line, (int)ret, (int)m_diagCount);
	    }
	}
	bool match = false;
	for (unsigned i = 0; i < m_expCount; i++) {
	    if (ret == m_expList[i].m_ret) {
		match = true;
		break;
	    }
	}
	if (! match) {
	    showDiags();
	    fatal("%s: thr %d line %d: ret=%d not expected", ccp->m_name, m_no, line, ret);
	}
	// reset expected to success
	exp(SQL_SUCCESS, 0, 0, true);
    }
    void chk(SQLSMALLINT type, SQLHANDLE handle, int line, bool match, const char* fmt, ...) {
	if (match)
	    return;
	va_list ap;
	va_start(ap, fmt);
	char buf[500];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	fatal("%s: thr %d line %d: check failed - %s", ccp->m_name, m_no, line, buf);
    }
};

#define HNull		0, SQL_NULL_HANDLE, __LINE__
#define HEnv(h)		SQL_HANDLE_ENV, h, __LINE__
#define HDbc(h)		SQL_HANDLE_DBC, h, __LINE__
#define HStmt(h)	SQL_HANDLE_STMT, h, __LINE__
#define HDesc(h)	SQL_HANDLE_DESC, h, __LINE__

// string support

#define MAX_SQL		20000

static void
scopy(char*& ptr, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsprintf(ptr, fmt, ap);
    va_end(ap);
    ptr += strlen(ptr);
}

static bool
blankeq(const char* s1, const char* s2, bool caseSensitive = false)
{
    unsigned n1 = strlen(s1);
    unsigned n2 = strlen(s2);
    unsigned i = 0;
    char c1 = 0;
    char c2 = 0;
    while (i < n1 || i < n2) {
	c1 = i < n1 ? s1[i] : 0x20;
	if (! caseSensitive && 'a' <= c1 && c1 <= 'z')
	    c1 -= 'a' - 'A';
	c2 = i < n2 ? s2[i] : 0x20;
	if (! caseSensitive && 'a' <= c2 && c2 <= 'z')
	    c2 -= 'a' - 'A';
	if (c1 != c2)
	    break;
	i++;
    }
    return c1 == c2;
}

// columns and tables

struct Col {
    enum Type {
	Char = SQL_CHAR,
	Varchar = SQL_VARCHAR,
	Int = SQL_INTEGER,
	Bigint = SQL_BIGINT,
	Real = SQL_REAL,
	Double = SQL_DOUBLE
    };
    enum CType {
	CChar = SQL_C_CHAR,
	CLong = SQL_C_SLONG,
	CDouble = SQL_C_DOUBLE
    };
    enum Cons {
	Null,		// nullable
	NotNull,	// not nullable
	Pk		// part of primary key
    };
    const char* m_name;
    Type m_type;
    unsigned m_length;
    Cons m_cons;
    CType m_ctype;
    Col() :
	m_type((Type)999) {
    }
    Col(const char* name, Type type, unsigned length, Cons cons, CType ctype) :
	m_name(name),
	m_type(type),
	m_length(length),
	m_cons(cons),
	m_ctype(ctype) {
    }
    unsigned size() const {
	switch (m_type) {
	case Char:
	case Varchar:
	    return m_length;
	case Int:
	    return 4;
	case Bigint:
	    return 8;
	case Real:
	    return 4;
	case Double:
	    return 8;
	}
	assert(false);
	return 0;
    }
    unsigned csize() const {	// size as char plus terminating null
	switch (m_ctype) {
	case CChar:
	    return m_length + 1;
	case CLong:
	    return 12;
	case CDouble:
	    return 24;
	}
	assert(false);
	return 0;
    }
    void typespec(char*& ptr) const {
	switch (m_type) {
	case Char:
	    scopy(ptr, "char(%d)", m_length);
	    return;
	case Varchar:
	    scopy(ptr, "varchar(%d)", m_length);
	    return;
	case Int:
	    scopy(ptr, "int");
	    return;
	case Bigint:
	    scopy(ptr, "bigint");
	    return;
	case Real:
	    scopy(ptr, "real");
	    return;
	case Double:
	    scopy(ptr, "float");
	    return;
	}
	assert(false);
    }
    SQLSMALLINT type() const {
	return (SQLSMALLINT)m_type;
    }
    SQLSMALLINT ctype() const {
	return (SQLSMALLINT)m_ctype;
    }
    void create(char*& ptr, bool pk) const {
	scopy(ptr, "%s", m_name);
	scopy(ptr, " ");
	typespec(ptr);
	if (m_cons == Pk && pk) {
	    scopy(ptr, " primary key");
	}
	if (m_cons == NotNull) {
	    scopy(ptr, " not null");
	}
    }
};

static Col ColUndef;

struct Tab {
    const char* m_name;
    const Col* m_colList;
    unsigned m_colCount;
    unsigned m_pkCount;
    unsigned* m_pkIndex;
    unsigned m_nkCount;
    unsigned* m_nkIndex;
    char m_upperName[20];
    Tab(const char* name, const Col* colList, unsigned colCount) :
	m_name(name),
	m_colList(colList),
	m_colCount(colCount) {
	m_pkCount = 0;
	m_nkCount = 0;
	for (unsigned i = 0; i < m_colCount; i++) {
	    const Col& col = m_colList[i];
	    if (col.m_cons == Col::Pk)
		m_pkCount++;
	    else
		m_nkCount++;
	}
	m_pkIndex = new unsigned[m_pkCount];
	m_nkIndex = new unsigned[m_nkCount];
	unsigned pk = 0;
	unsigned nk = 0;
	for (unsigned i = 0; i < m_colCount; i++) {
	    const Col& col = m_colList[i];
	    if (col.m_cons == Col::Pk)
		m_pkIndex[pk++] = i;
	    else
		m_nkIndex[nk++] = i;
	}
	assert(pk == m_pkCount && nk == m_nkCount);
	strcpy(m_upperName, m_name);
	for (char* p = m_upperName; *p != 0; p++) {
	    if ('a' <= *p && *p <= 'z')
		*p -= 'a' - 'A';
	}
    }
    ~Tab() {
	delete[] m_pkIndex;
	delete[] m_nkIndex;
    }
    void drop(char*& ptr) const {
	scopy(ptr, "drop table %s", m_name);
    }
    void create(char*& ptr) const {
	scopy(ptr, "create table %s (", m_name);
	for (unsigned i = 0; i < m_colCount; i++) {
	    if (i > 0)
		scopy(ptr, ", ");
	    const Col& col = m_colList[i];
	    col.create(ptr, m_pkCount == 1);
	}
	if (m_pkCount != 1) {
	    scopy(ptr, ", primary key (");
	    for (unsigned i = 0; i < m_pkCount; i++) {
		const Col& col = m_colList[m_pkIndex[i]];
		if (i > 0)
		    scopy(ptr, ", ");
		scopy(ptr, "%s", col.m_name);
	    }
	    scopy(ptr, ")");
	}
	scopy(ptr, ")");
    }
    void wherePk(char*& ptr) const {
	scopy(ptr, " where");
	for (unsigned i = 0; i < m_pkCount; i++) {
	    const Col& col = m_colList[m_pkIndex[i]];
	    if (i > 0)
		scopy(ptr, " and");
	    scopy(ptr, " %s = ?", col.m_name);
	}
    }
    void whereRange(char*& ptr) const {
	scopy(ptr, " where");
	for (unsigned i = 0; i < m_pkCount; i++) {
	    const Col& col = m_colList[m_pkIndex[i]];
	    if (i > 0)
		scopy(ptr, " and");
	    scopy(ptr, " ? <= %s", col.m_name);
	    scopy(ptr, " and ");
	    scopy(ptr, "%s < ?", col.m_name);
	}
    }
    void orderPk(char*& ptr) const {
	scopy(ptr, " order by");
	for (unsigned i = 0; i < m_pkCount; i++) {
	    const Col& col = m_colList[m_pkIndex[i]];
	    if (i > 0)
		scopy(ptr, ", ");
	    else
		scopy(ptr, " ");
	    scopy(ptr, "%s", col.m_name);
	}
    }
    void selectPk(char*& ptr) const {
	scopy(ptr, "select * from %s", m_name);
	wherePk(ptr);
    }
    void selectAll(char*& ptr) const {
	scopy(ptr, "select * from %s", m_name);
    }
    void selectRange(char*& ptr, bool sort) const {
	selectAll(ptr);
	whereRange(ptr);
	if (sort)
	    orderPk(ptr);
    }
    void selectCount(char*& ptr) const {
	scopy(ptr, "select count(*) from %s", m_name);
    }
    void insertAll(char*& ptr) const {
	scopy(ptr, "insert into %s values (", m_name);
	for (unsigned i = 0; i < m_colCount; i++) {
	    if (i > 0)
		scopy(ptr, ", ");
	    scopy(ptr, "?");
	}
	scopy(ptr, ")");
    }
    void updatePk(char*& ptr) const {
	scopy(ptr, "update %s set", m_name);
	for (unsigned i = 0; i < m_nkCount; i++) {
	    const Col& col = m_colList[m_nkIndex[i]];
	    if (i > 0)
		scopy(ptr, ", ");
	    else
		scopy(ptr, " ");
	    scopy(ptr, "%s = ?", col.m_name);
	}
	wherePk(ptr);
    }
    void updateRange(char*& ptr) const {
	scopy(ptr, "update %s set", m_name);
	for (unsigned i = 0; i < m_nkCount; i++) {
	    const Col& col = m_colList[m_nkIndex[i]];
	    if (i > 0)
		scopy(ptr, ", ");
	    else
		scopy(ptr, " ");
	    scopy(ptr, "%s = ?", col.m_name);		// XXX constant for now
	}
	whereRange(ptr);
    }
    void deleteAll(char*& ptr) const {
	scopy(ptr, "delete from %s", m_name);
    }
    void deletePk(char*& ptr) const {
	scopy(ptr, "delete from %s", m_name);
	wherePk(ptr);
    }
    void deleteRange(char*& ptr) const {
	scopy(ptr, "delete from %s", m_name);
	whereRange(ptr);
    }
    // simple
    void insertDirect(char*& ptr, unsigned n) const {
	scopy(ptr, "insert into %s values (", m_name);
	for (unsigned i = 0; i < m_colCount; i++) {
	    const Col& col = m_colList[i];
	    if (i > 0)
		scopy(ptr, ", ");
	    if (col.m_type == Col::Char || col.m_type == Col::Varchar) {
		scopy(ptr, "'");
		for (unsigned i = 0; i <= n % col.m_length; i++)
		    scopy(ptr, "%c", 'a' + (n + i) % 26);
		scopy(ptr, "'");
	    } else if (col.m_type == Col::Int || col.m_type == Col::Bigint) {
		scopy(ptr, "%u", n);
	    } else if (col.m_type == Col::Real || col.m_type == Col::Double) {
		scopy(ptr, "%.3f", n * 0.001);
	    } else {
		assert(false);
	    }
	}
	scopy(ptr, ")");
    }
    void whereDirect(char*& ptr, unsigned n) const {
	scopy(ptr, " where");
	for (unsigned i = 0; i < m_pkCount; i++) {
	    const Col& col = m_colList[m_pkIndex[i]];
	    if (i > 0)
		scopy(ptr, ", ");
	    else
		scopy(ptr, " ");
	    scopy(ptr, "%s = ", col.m_name);
	    if (col.m_type == Col::Char || col.m_type == Col::Varchar) {
		scopy(ptr, "'");
		for (unsigned i = 0; i <= n % col.m_length; i++)
		    scopy(ptr, "%c", 'a' + (n + i) % 26);
		scopy(ptr, "'");
	    } else if (col.m_type == Col::Int || col.m_type == Col::Bigint) {
		scopy(ptr, "%u", n);
	    } else {
		assert(false);
	    }
	}
    }
    void countDirect(char*& ptr, unsigned n) const {
	scopy(ptr, "select count(*) from %s", m_name);
	whereDirect(ptr, n);
    }
    void deleteDirect(char*& ptr, unsigned n) const {
	scopy(ptr, "delete from %s", m_name);
	whereDirect(ptr, n);
    }
    // joins
    void selectCart(char*& ptr, unsigned cnt) const {
	scopy(ptr, "select count(*) from");
	for (unsigned j = 0; j < cnt; j++) {
	    if (j > 0)
		scopy(ptr, ",");
	    scopy(ptr, " %s", m_name);
	    scopy(ptr, " t%u", j);
	}
    }
    void selectJoin(char*& ptr, unsigned cnt) const {
	scopy(ptr, "select * from");
	for (unsigned j = 0; j < cnt; j++) {
	    if (j > 0)
		scopy(ptr, ",");
	    scopy(ptr, " %s", m_name);
	    scopy(ptr, " t%u", j);
	}
	for (unsigned i = 0; i < m_pkCount; i++) {
	    const Col& col = m_colList[m_pkIndex[i]];
	    for (unsigned j = 0; j < cnt - 1; j++) {
		if (i == 0 && j == 0)
		    scopy(ptr, " where");
		else
		    scopy(ptr, " and");
		scopy(ptr, " t%u.%s = t%u.%s", j, col.m_name, j + 1, col.m_name);
	    }
	}
    }
    // check if selected on command line
    bool optok() const {
	return opt.m_table == 0 || strcasecmp(m_name, opt.m_table) == 0;
    }
};

// the test tables

static Col col0[] = {
    Col( "a",	Col::Bigint,	0,	Col::Pk,	Col::CLong	),
    Col( "b",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c",	Col::Char,	4,	Col::NotNull,	Col::CChar	),
    Col( "d",	Col::Double,	0,	Col::Null,	Col::CDouble	),
    Col( "e",	Col::Char,	40,	Col::Null,	Col::CChar	),
    Col( "f",	Col::Char,	10,	Col::Null,	Col::CChar	)
};

static Col col1[] = {
    Col( "c0",	Col::Int,	0,	Col::Pk,	Col::CLong	),
    Col( "c1",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c2",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c3",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c4",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c5",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c6",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c7",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c8",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c9",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c10",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c11",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c12",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c13",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c14",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c15",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c16",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c17",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c18",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c19",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c20",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c21",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c22",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c23",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c24",	Col::Int,	0,	Col::NotNull,	Col::CLong	),
    Col( "c25",	Col::Int,	0,	Col::NotNull,	Col::CLong	)
};

static Col col2[] = {
    Col( "a",	Col::Int,	0,	Col::Pk,	Col::CLong	),
    Col( "c",	Col::Char,	8000,	Col::NotNull,	Col::CChar	)
};

static Col col3[] = {
    Col( "a",	Col::Int,	0,	Col::Pk,	Col::CLong	),
    Col( "c1",	Col::Varchar,	1,	Col::Null,	Col::CChar	),
    Col( "c2",	Col::Varchar,	2,	Col::Null,	Col::CChar	),
    Col( "c3",	Col::Varchar,	3,	Col::Null,	Col::CChar	),
    Col( "c4",	Col::Varchar,	4,	Col::Null,	Col::CChar	),
    Col( "c5",	Col::Varchar,	10,	Col::Null,	Col::CChar	),
    Col( "c6",	Col::Varchar,	40,	Col::Null,	Col::CChar	),
    Col( "c7",	Col::Varchar,	255,	Col::Null,	Col::CChar	),
    Col( "c8",	Col::Varchar,	4000,	Col::Null,	Col::CChar	)
};

static Col col4[] = {
    Col( "a",	Col::Char,	8,	Col::Pk,	Col::CChar	),
    Col( "b",	Col::Char,	8,	Col::NotNull,	Col::CChar	),
};

static Tab tabList[] = {
#define colList(x)	x, arraySize(x)
    Tab( "tt00",	colList(col0)	),
    Tab( "tt01",	colList(col1)	),	// fläskbench special
    Tab( "tt02",	colList(col2)	),
    Tab( "tt03",	colList(col3)	),
    Tab( "tt04",	colList(col4)	)
#undef colList
};

static const unsigned tabCount = arraySize(tabList);
static const unsigned maxColCount = 100;	// per table - keep up to date

static bool
findTable()
{
    for (unsigned i = 0; i < tabCount; i++) {
	const Tab& tab = tabList[i];
	if (tab.optok())
	    return true;
    }
    return false;
}

static void
listTables()
{
    ndbout << "tables:" << endl;
    for (unsigned i = 0; i < tabCount; i++) {
	const Tab& tab = tabList[i];
	if (i > 0)
	    ndbout << " ";
	ndbout << tab.m_name;
    }
    ndbout << endl;
}

// data fields and rows

struct Fld {
    const Col& m_col;
    union {
	char* m_char;
	long m_long;
	double m_double;
    };
    SQLINTEGER m_ind;
    SQLINTEGER m_need;		// constant
    Fld() :
	m_col(ColUndef),
	m_need(0) {
    }
    Fld(const Col& col) :
	m_col(col),
	m_need(SQL_LEN_DATA_AT_EXEC(0)) {
	switch (m_col.m_ctype) {
	case Col::CChar:
	    m_char = new char[m_col.csize()];
	    memset(m_char, 0, m_col.csize());
	    break;
	case Col::CLong:
	    m_long = 0;
	    break;
	case Col::CDouble:
	    m_double = 0.0;
	    break;
	}
	m_ind = -1;
    }
    ~Fld() {
	switch (m_col.m_ctype) {
	case Col::CChar:
	    delete[] m_char;
	    break;
	case Col::CLong:
	    break;
	case Col::CDouble:
	    break;
	}
    }
    void zero() {
	switch (m_col.m_ctype) {
	case Col::CChar:
	    memset(m_char, 0x1f, m_col.csize());
	    break;
	case Col::CLong:
	    m_long = 0x1f1f1f1f;
	    break;
	case Col::CDouble:
	    m_double = 1111111.1111111;
	    break;
	}
	m_ind = -1;
    }
    // copy values from another field
    void copy(const Fld& fld) {
	assert(&m_col == &fld.m_col);
	switch (m_col.m_ctype) {
	case Col::CChar:
	    memcpy(m_char, fld.m_char, m_col.csize());
	    break;
	case Col::CLong:
	    m_long = fld.m_long;
	    break;
	case Col::CDouble:
	    m_double = fld.m_double;
	    break;
	default:
	    assert(false);
	    break;
	}
	m_ind = fld.m_ind;
    }
    SQLPOINTER caddr() {
	switch (m_col.m_ctype) {
	case Col::CChar:
	    return (SQLPOINTER)m_char;
	case Col::CLong:
	    return (SQLPOINTER)&m_long;
	case Col::CDouble:
	    return (SQLPOINTER)&m_double;
	}
	assert(false);
	return 0;
    }
    SQLINTEGER* ind() {
	return &m_ind;
    }
    SQLINTEGER* need() {
	m_need = SQL_LEN_DATA_AT_EXEC(0);
	return &m_need;
    }
    void calcPk(const Test& test, unsigned rownum) {
	switch (m_col.m_ctype) {
	case Col::CChar:
	    test.calcPk(rownum, m_char, m_col.csize());
	    m_ind = SQL_NTS;
	    return;
	case Col::CLong:
	    test.calcPk(rownum, &m_long);
	    m_ind = 0;
	    return;
	case Col::CDouble:
	    assert(false);
	    return;
	}
	assert(false);
    }
    void hashPk(const Test& test, unsigned* hash) const {
	switch (m_col.m_ctype) {
	case Col::CChar:
	    test.hashPk(hash, m_char, m_col.csize());
	    return;
	case Col::CLong:
	    test.hashPk(hash, m_long);
	    return;
	case Col::CDouble:
	    assert(false);
	    return;
	}
	assert(false);
    }
    void calcNk(const Test& test, unsigned hash) {
	bool null = m_col.m_cons == Col::Null;
	switch (m_col.m_ctype) {
	case Col::CChar:
	    test.calcNk(hash, m_char, m_col.csize(), &m_ind, null);
	    return;
	case Col::CLong:
	    test.calcNk(hash, &m_long, &m_ind, null);
	    return;
	case Col::CDouble:
	    test.calcNk(hash, &m_double, &m_ind, null);
	    return;
	}
	assert(false);
    }
    bool verify(Test& test, const Fld& fld) {
	assert(&m_col == &fld.m_col);
	switch (m_col.m_ctype) {
	case Col::CChar:
	    return test.verify(m_char, m_ind, fld.m_char, fld.m_ind, m_col.csize());
	case Col::CLong:
	    return test.verify(m_long, m_ind, fld.m_long, fld.m_ind);
	case Col::CDouble:
	    return test.verify(m_double, m_ind, fld.m_double, fld.m_ind);
	}
	assert(false);
	return false;
    }
    // debug
    void print() const {
	if (m_ind == SQL_NULL_DATA)
	    ndbout << "NULL";
	else {
	    switch (m_col.m_ctype) {
	    case Col::CChar:
		ndbout << m_char;
		break;
	    case Col::CLong:
		ndbout << (int)m_long;
		break;
	    case Col::CDouble:
		ndbout << m_double;
		break;
	    }
	}
    }
};

struct Row {
    const Tab& m_tab;
    Fld* m_fldList;
    Row(const Tab& tab) :
	m_tab(tab) {
	m_fldList = new Fld[m_tab.m_colCount];
	for (unsigned i = 0; i < m_tab.m_colCount; i++) {
	    const Col& col = m_tab.m_colList[i];
	    void* place = &m_fldList[i];
	    new (place) Fld(col);
	}
    }
    ~Row() {
	delete[] m_fldList;
    }
    // copy values from another row
    void copy(const Row& row) {
	assert(&m_tab == &row.m_tab);
	for (unsigned i = 0; i < m_tab.m_colCount; i++) {
	    Fld& fld = m_fldList[i];
	    fld.copy(row.m_fldList[i]);
	}
    }
    // primary key value is determined by row number
    void calcPk(Test& test, unsigned rownum) {
	for (unsigned i = 0; i < m_tab.m_pkCount; i++) {
	    Fld& fld = m_fldList[m_tab.m_pkIndex[i]];
	    fld.calcPk(test, rownum);
	}
    }
    // other fields are determined by primary key value
    void calcNk(Test& test) {
	unsigned hash = test.m_salt;
	for (unsigned i = 0; i < m_tab.m_pkCount; i++) {
	    Fld& fld = m_fldList[m_tab.m_pkIndex[i]];
	    fld.hashPk(test, &hash);
	}
	for (unsigned i = 0; i < m_tab.m_colCount; i++) {
	    const Col& col = m_tab.m_colList[i];
	    if (col.m_cons == Col::Pk)
		continue;
	    Fld& fld = m_fldList[i];
	    fld.calcNk(test, hash);
	}
    }
    // verify against another row
    bool verifyPk(Test& test, const Row& row) const {
	assert(&m_tab == &row.m_tab);
	for (unsigned i = 0; i < m_tab.m_pkCount; i++) {
	    Fld& fld = m_fldList[m_tab.m_pkIndex[i]];
	    if (! fld.verify(test, row.m_fldList[m_tab.m_pkIndex[i]])) {
		ndbout << "verify failed: tab=" << m_tab.m_name << " col=" << fld.m_col.m_name << endl;
		return false;
	    }
	}
	return true;
    }
    bool verifyNk(Test& test, const Row& row) const {
	assert(&m_tab == &row.m_tab);
	for (unsigned i = 0; i < m_tab.m_nkCount; i++) {
	    Fld& fld = m_fldList[m_tab.m_nkIndex[i]];
	    if (! fld.verify(test, row.m_fldList[m_tab.m_nkIndex[i]])) {
		ndbout << "verify failed: tab=" << m_tab.m_name << " col=" << fld.m_col.m_name << endl;
		return false;
	    }
	}
	return true;
    }
    bool verify(Test& test, const Row& row) const {
	return verifyPk(test, row) && verifyNk(test, row);
    }
    // debug
    void print() const {
	ndbout << "row";
	for (unsigned i = 0; i < m_tab.m_colCount; i++) {
	    ndbout << " " << i << "=";
	    Fld& fld = m_fldList[i];
	    fld.print();
	}
	ndbout << endl;
    }
};

// set ODBC version - required

static void
setVersion(Test& test, SQLHANDLE hEnv)
{
    test.run(HEnv(hEnv), SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_INTEGER));
}

// set autocommit

static void
setAutocommit(Test& test, SQLHANDLE hDbc, bool on)
{
    SQLUINTEGER value = on ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF;
    test.run(HDbc(hDbc), SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)value, SQL_IS_UINTEGER));
    SQLUINTEGER value2 = (SQLUINTEGER)-1;
    test.run(HDbc(hDbc), SQLGetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)&value2, SQL_IS_UINTEGER, 0));
    test.chk(HDbc(hDbc), value2 == value, "got %u != %u", (unsigned)value2, (unsigned)value);
}

// subroutines - migrate simple common routines here

static void
allocEnv(Test& test, SQLHANDLE& hEnv)
{
    test.run(HNull, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv));
    setVersion(test, hEnv);
}

static void
allocDbc(Test& test, SQLHANDLE hEnv, SQLHANDLE& hDbc)
{
    test.run(HEnv(hEnv), SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc));
}

static void
allocConn(Test& test, SQLHANDLE hEnv, SQLHANDLE& hDbc)
{
    allocDbc(test, hEnv, hDbc);
#ifdef unixODBC
    test.exp(SQL_SUCCESS_WITH_INFO, "IM003", 0, false);		// unicode??
    test.exp(SQL_SUCCESS_WITH_INFO, "01000", 0, false);		// version??
#endif
    test.run(HDbc(hDbc), SQLConnect(hDbc, (SQLCHAR*)opt.m_dsn, SQL_NTS, (SQLCHAR*)"user", SQL_NTS, (SQLCHAR*)"pass", SQL_NTS));
}

static void
allocStmt(Test& test, SQLHANDLE hDbc, SQLHANDLE& hStmt)
{
    test.run(HDbc(hDbc), SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt));
}

static void
allocAll(Test& test, SQLHANDLE& hEnv, SQLHANDLE& hDbc, SQLHANDLE& hStmt)
{
    allocEnv(test, hEnv);
    allocConn(test, hEnv, hDbc);
    allocStmt(test, hDbc, hStmt);
}

static void
allocAll(Test& test, SQLHANDLE& hEnv, SQLHANDLE& hDbc, SQLHANDLE* hStmtList, unsigned nStmt)
{
    allocEnv(test, hEnv);
    allocConn(test, hEnv, hDbc);
    for (unsigned i = 0; i < nStmt; i++)
	allocStmt(test, hDbc, hStmtList[i]);
}

static void
freeEnv(Test& test, SQLHANDLE hEnv)
{
    test.run(HNull, SQLFreeHandle(SQL_HANDLE_ENV, hEnv));
}

static void
freeDbc(Test& test, SQLHANDLE hEnv, SQLHANDLE hDbc)
{
    test.run(HEnv(hEnv), SQLFreeHandle(SQL_HANDLE_DBC, hDbc));
}

static void
freeConn(Test& test, SQLHANDLE hEnv, SQLHANDLE hDbc)
{
    test.run(HDbc(hDbc), SQLDisconnect(hDbc));
    test.run(HEnv(hEnv), SQLFreeHandle(SQL_HANDLE_DBC, hDbc));
}

static void
freeStmt(Test& test, SQLHANDLE hDbc, SQLHANDLE hStmt)
{
    test.run(HDbc(hDbc), SQLFreeHandle(SQL_HANDLE_STMT, hStmt));
}

static void
freeAll(Test& test, SQLHANDLE hEnv, SQLHANDLE hDbc, SQLHANDLE hStmt)
{
    freeStmt(test, hDbc, hStmt);
    freeConn(test, hEnv, hDbc);
    freeEnv(test, hEnv);
}

static void
freeAll(Test& test, SQLHANDLE hEnv, SQLHANDLE hDbc, SQLHANDLE* hStmtList, unsigned nStmt)
{
    for (unsigned i = 0; i < nStmt; i++)
	freeStmt(test, hDbc, hStmtList[i]);
    freeConn(test, hEnv, hDbc);
    freeEnv(test, hEnv);
}

#define chkTuplesFetched(/*Test&*/ _test, /*SQLHANDLE*/ _hStmt, /*SQLUINTEGER*/ _countExp) \
do { \
    SQLUINTEGER _count = (SQLUINTEGER)-1; \
    getTuplesFetched(_test, _hStmt, &_count); \
    test.chk(HStmt(_hStmt), _count == _countExp, "tuples: got %ld != %ld", (long)_count, (long)_countExp); \
} while (0)

static void
getTuplesFetched(Test& test, SQLHANDLE hStmt, SQLUINTEGER* count)
{
    *count = (SQLUINTEGER)-1;
    test.run(HStmt(hStmt), SQLGetStmtAttr(hStmt, SQL_ATTR_NDB_TUPLES_FETCHED, count, SQL_IS_POINTER, 0));
}

static void
selectCount(Test& test, SQLHANDLE hStmt, const char* sql, long* count)
{
    if (opt.m_v >= 3)
	ndbout << "SQL: " << sql << endl;
    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
    SQLINTEGER ind;
    test.run(HStmt(hStmt), SQLBindCol(hStmt, 1, SQL_C_SLONG, count, 0, &ind));
    ind = -1;
    *count = -1;
    test.run(HStmt(hStmt), SQLExecute(hStmt));
    unsigned k = 0;
    while (1) {
	if (k == 1)
	    test.exp(SQL_NO_DATA, 0, 0, true);
	test.run(HStmt(hStmt), SQLFetch(hStmt));
	if (k == 1)
	    break;
	k++;
    }
    test.chk(HStmt(hStmt), ind == sizeof(long), "got %d != %d", (int)ind, (int)sizeof(long));
    test.chk(HStmt(hStmt), *count >= 0, "got %ld < 0", *count);
    chkTuplesFetched(test, hStmt, *count);
#ifndef iODBC
    //
    test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
#else
    test.run(HStmt(hStmt), SQLFreeStmt(hStmt, SQL_CLOSE));
#endif
}

static void
selectCount(Test& test, SQLHANDLE hStmt, const Tab& tab, long* count)
{
    static char sql[MAX_SQL], *sqlptr;	// XXX static or core
    tab.selectCount(sqlptr = sql);
    selectCount(test, hStmt, sql, count);
}

static void
verifyCount(Test& test, SQLHANDLE hStmt, const Tab& tab, long countExp)
{
    long count = -1;
    selectCount(test, hStmt, tab, &count);
    test.chk(HStmt(hStmt), count == countExp, "got %ld != %ld", count, countExp);
}

#define chkRowCount(/*Test&*/ _test, /*SQLHANDLE*/ _hStmt, /*SQLINTEGER*/ _countExp) \
do { \
    SQLINTEGER _count = -1; \
    getRowCount(_test, _hStmt, &_count); \
    test.chk(HStmt(_hStmt), _count == _countExp, "rowcount: got %ld != %ld", (long)_count, (long)_countExp); \
} while (0)

static void
getRowCount(Test& test, SQLHANDLE hStmt, SQLINTEGER* count)
{
    *count = -1;
    test.run(HStmt(hStmt), SQLRowCount(hStmt, count));
}

// handle allocation

static void
testAlloc(Test& test)
{
    const unsigned n1 = (opt.m_scale >> 8) & 0xf;	// default 500 = 0x1f4
    const unsigned n2 = (opt.m_scale >> 4) & 0xf;
    const unsigned n3 = (opt.m_scale >> 0) & 0xf;
    const unsigned count = n1 + n1 * n2 + n1 * n2 * n3;
    SQLHANDLE hEnvList[0xf];
    SQLHANDLE hDbcList[0xf][0xf];
    SQLHANDLE hStmtList[0xf][0xf][0xf];
    for (unsigned i1 = 0; i1 < n1; i1++) {
	SQLHANDLE& hEnv = hEnvList[i1];
	test.run(HNull, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv));
	test.run(HEnv(hEnv), SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_INTEGER));
	for (unsigned i2 = 0; i2 < n2; i2++) {
	    SQLHANDLE& hDbc = hDbcList[i1][i2];
	    test.run(HEnv(hEnv), SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc));
#ifdef unixODBC
	    test.exp(SQL_SUCCESS_WITH_INFO, "IM003", 0, false);		// unicode??
	    test.exp(SQL_SUCCESS_WITH_INFO, "01000", 0, false);		// version??
#endif
	    test.run(HDbc(hDbc), SQLConnect(hDbc, (SQLCHAR*)opt.m_dsn, SQL_NTS, (SQLCHAR*)"user", SQL_NTS, (SQLCHAR*)"pass", SQL_NTS));
	    // some attributes
	    test.exp(SQL_ERROR, "HY092", -1, true);	// read-only attribute
	    test.run(HDbc(hDbc), SQLSetConnectAttr(hDbc, SQL_ATTR_AUTO_IPD, (SQLPOINTER)SQL_TRUE, SQL_IS_UINTEGER));
	    test.exp(SQL_ERROR, "HYC00", -1, true);	// not supported
	    test.run(HDbc(hDbc), SQLSetConnectAttr(hDbc, SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TXN_SERIALIZABLE, SQL_IS_UINTEGER));
	    test.run(HDbc(hDbc), SQLSetConnectAttr(hDbc, SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)"DEFAULT", strlen("DEFAULT")));
	    for (unsigned i3 = 0; i3 < n3; i3++) {
		SQLHANDLE& hStmt = hStmtList[i1][i2][i3];
		test.run(HDbc(hDbc), SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt));
		SQLHANDLE ipd0, ipd1;
		SQLHANDLE ird0, ird1;
		SQLHANDLE apd0, apd1, apd2;
		SQLHANDLE ard0, ard1, ard2;
		// get
		ipd0 = ird0 = apd0 = ard0 = 0;
		test.run(HStmt(hStmt), SQLGetStmtAttr(hStmt, SQL_ATTR_IMP_PARAM_DESC, &ipd0, SQL_IS_POINTER, 0));
		test.run(HStmt(hStmt), SQLGetStmtAttr(hStmt, SQL_ATTR_IMP_ROW_DESC, &ird0, SQL_IS_POINTER, 0));
		test.run(HStmt(hStmt), SQLGetStmtAttr(hStmt, SQL_ATTR_APP_PARAM_DESC, &apd0, SQL_IS_POINTER, 0));
		test.run(HStmt(hStmt), SQLGetStmtAttr(hStmt, SQL_ATTR_APP_ROW_DESC, &ard0, SQL_IS_POINTER, 0));
#ifndef unixODBC
		test.chk(HStmt(hStmt), ipd0 != 0, "got 0");
		test.chk(HStmt(hStmt), ird0 != 0, "got 0");
		test.chk(HStmt(hStmt), apd0 != 0, "got 0");
		test.chk(HStmt(hStmt), ard0 != 0, "got 0");
#endif
		// alloc
		ipd1 = ird1 = apd1 = ard1 = 0;
		test.run(HDbc(hDbc), SQLAllocHandle(SQL_HANDLE_DESC, hDbc, &ipd1));
		test.run(HDbc(hDbc), SQLAllocHandle(SQL_HANDLE_DESC, hDbc, &ird1));
		test.run(HDbc(hDbc), SQLAllocHandle(SQL_HANDLE_DESC, hDbc, &apd1));
		test.run(HDbc(hDbc), SQLAllocHandle(SQL_HANDLE_DESC, hDbc, &ard1));
		test.chk(HDbc(hDbc), ipd1 != 0 && ird1 != 0 && apd1 != 0 && ard1 != 0, "got null");
		// set
		test.exp(SQL_ERROR, "HY092", -1, true);	// read-only attribute
		test.run(HStmt(hStmt), SQLSetStmtAttr(hStmt, SQL_ATTR_IMP_PARAM_DESC, ipd1, SQL_IS_POINTER));
		test.exp(SQL_ERROR, "HY092", -1, true);	// read-only attribute
		test.run(HStmt(hStmt), SQLSetStmtAttr(hStmt, SQL_ATTR_IMP_ROW_DESC, ird1, SQL_IS_POINTER));
		test.run(HStmt(hStmt), SQLSetStmtAttr(hStmt, SQL_ATTR_APP_PARAM_DESC, apd1, SQL_IS_POINTER));
		test.run(HStmt(hStmt), SQLSetStmtAttr(hStmt, SQL_ATTR_APP_ROW_DESC, ard1, SQL_IS_POINTER));
		// get

		apd2 = ard2 = 0;
		test.run(HStmt(hStmt), SQLGetStmtAttr(hStmt, SQL_ATTR_APP_PARAM_DESC, &apd2, SQL_IS_POINTER, 0));
		test.run(HStmt(hStmt), SQLGetStmtAttr(hStmt, SQL_ATTR_APP_ROW_DESC, &ard2, SQL_IS_POINTER, 0));
		test.chk(HStmt(hStmt), apd2 == apd1, "got %x != %x", (unsigned)apd2, (unsigned)apd1);
		test.chk(HStmt(hStmt), ard2 == ard1, "got %x != %x", (unsigned)ard2, (unsigned)ard1);
		// free
		test.run(HDbc(hDbc), SQLFreeHandle(SQL_HANDLE_DESC, ipd1));
		test.run(HDbc(hDbc), SQLFreeHandle(SQL_HANDLE_DESC, ird1));
		test.run(HDbc(hDbc), SQLFreeHandle(SQL_HANDLE_DESC, apd1));
		test.run(HDbc(hDbc), SQLFreeHandle(SQL_HANDLE_DESC, ard1));
	    }
	}
    }
    test.timerCnt(count);
    if (opt.m_v >= 3)
	ndbout << "allocated " << count << endl;
    for (unsigned i1 = 0; i1 < n1; i1++) {
	SQLHANDLE& hEnv = hEnvList[i1];
	for (unsigned i2 = 0; i2 < n2; i2++) {
	    SQLHANDLE& hDbc = hDbcList[i1][i2];
	    if (i2 % 2 == 0) {
		for (unsigned i3 = 0; i3 < n3; i3++) {
		    SQLHANDLE& hStmt = hStmtList[i1][i2][i3];
		    test.run(HDbc(hDbc), SQLFreeHandle(SQL_HANDLE_STMT, hStmt));
		}
	    } else {
		// cleaned up by SQLDisconnect
	    }
	    test.run(HDbc(hDbc), SQLDisconnect(hDbc));
	    test.run(HEnv(hEnv), SQLFreeHandle(SQL_HANDLE_DBC, hDbc));
	}
	test.run(HNull, SQLFreeHandle(SQL_HANDLE_ENV, hEnv));
    }
    test.timerCnt(count);
    if (opt.m_v >= 3)
	ndbout << "freed " << count << endl;
}

// create tables

static void
testCreate(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	// drop
	tab.drop(sqlptr = sql);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	test.exp(SQL_ERROR, "IM000", 2040709, false);
	test.run(HStmt(hStmt), SQLExecute(hStmt));
	if (test.m_ret == SQL_SUCCESS)
	    test.chk(HStmt(hStmt), test.m_functionCode == SQL_DIAG_DROP_TABLE, "got %d != %d", test.m_functionCode, SQL_DIAG_DROP_TABLE);
	if (test.m_ret == SQL_SUCCESS && opt.m_v >= 2)
	    ndbout << "table " << tab.m_name << " dropped" << endl;
	if (test.m_ret != SQL_SUCCESS && opt.m_v >= 2)
	    ndbout << "table " << tab.m_name << " does not exist" << endl;
	test.timerCnt(1);
	// create
	tab.create(sqlptr = sql);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	test.exp(SQL_ERROR, "IM000", 2040721, false);
	test.run(HStmt(hStmt), SQLExecute(hStmt));
	if (test.m_ret == SQL_SUCCESS)
	    test.chk(HStmt(hStmt), test.m_functionCode == SQL_DIAG_CREATE_TABLE, "got %d != %d", test.m_functionCode, SQL_DIAG_CREATE_TABLE);
	if (test.m_ret == SQL_SUCCESS && opt.m_v >= 2)
	    ndbout << "table " << tab.m_name << " created" << endl;
	if (test.m_ret != SQL_SUCCESS && opt.m_v >= 2)
	    ndbout << "table " << tab.m_name << " already exists" << endl;
	test.timerCnt(1);
    }
    freeAll(test, hEnv, hDbc, hStmt);
}

// prepare without execute

static void
testPrepare(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned cnt = opt.m_depth; cnt <= opt.m_depth; cnt++) {
	for (unsigned i = 0; i < tabCount; i++) {
	    Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    tab.selectJoin(sqlptr = sql, cnt);
	    if (opt.m_v >= 2)
		ndbout << "SQL: " << sql << endl;
	    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    SQLSMALLINT colCount = -1;
	    SQLSMALLINT colExp = cnt * tab.m_colCount;
	    test.run(HStmt(hStmt), SQLNumResultCols(hStmt, &colCount));
	    test.chk(HStmt(hStmt), colCount == colExp, "got %d != %d", (int)colCount, (int)colExp);
	    test.timerCnt(1);
	}
    }
    freeAll(test, hEnv, hDbc, hStmt);
}

// catalog functions

static void
testCatalog(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    odbc_typeinfo: {
	long type[] = {
	    SQL_CHAR, SQL_VARCHAR, SQL_SMALLINT, SQL_INTEGER, SQL_BIGINT, SQL_REAL, SQL_DOUBLE
	};
	unsigned rows[] = {
	    1, 1, 2, 2, 2, 1, 1		// 2 for signed and unsigned
	};
	for (unsigned i = 0; i < arraySize(type); i++) {
	    test.run(HStmt(hStmt), SQLGetTypeInfo(hStmt, type[i]));
	    long dataType = 0;
	    test.run(HStmt(hStmt), SQLBindCol(hStmt, 2, SQL_C_SLONG, &dataType, 0, 0));
	    unsigned k = 0;
	    while (1) {
		if (k == rows[i])
		    test.exp(SQL_NO_DATA, 0, 0, true);
		test.run(HStmt(hStmt), SQLFetch(hStmt));
		if (k == rows[i])
		    break;
		test.chk(HStmt(hStmt), dataType == type[i], "got %ld != %ld", dataType, type[i]);
		test.timerCnt(1);
		k++;
	    }
#ifndef iODBC
	    test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
#else
	    freeStmt(test, hDbc, hStmt);
	    allocStmt(test, hDbc, hStmt);
#endif
	}
	if (opt.m_v >= 2)
	    ndbout << "found " << (UintPtr)arraySize(type) << " data types" << endl;
	test.run(HStmt(hStmt), SQLFreeStmt(hStmt, SQL_UNBIND));
    }
    odbc_tables: {
	unsigned found[tabCount];
	for (unsigned i = 0; i < tabCount; i++)
	    found[i] = 0;
	test.run(HStmt(hStmt), SQLTables(hStmt, (SQLCHAR*)0, 0, (SQLCHAR*)0, 0, (SQLCHAR*)0, 0, (SQLCHAR*)0, 0));
	char tableName[200] = "";
	char tableType[200] = "";
	test.run(HStmt(hStmt), SQLBindCol(hStmt, 3, SQL_C_CHAR, tableName, sizeof(tableName), 0));
	test.run(HStmt(hStmt), SQLBindCol(hStmt, 4, SQL_C_CHAR, tableType, sizeof(tableType), 0));
	unsigned cnt = 0;
	while (1) {
	    test.exp(SQL_NO_DATA, 0, 0, false);
	    test.run(HStmt(hStmt), SQLFetch(hStmt));
	    if (test.m_ret == SQL_NO_DATA)
		break;
	    test.timerCnt(1);
	    cnt++;
	    if (! blankeq(tableType, "TABLE"))
		continue;
	    for (unsigned i = 0; i < tabCount; i++) {
		const Tab& tab = tabList[i];
		if (! tab.optok())
		    continue;
		if (! blankeq(tab.m_name, tableName))
		    continue;
		test.chk(HStmt(hStmt), found[i] == 0, "duplicate table %s", tab.m_name);
		found[i]++;
	    }
	}
#ifndef iODBC
	test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
#else
	freeStmt(test, hDbc, hStmt);
	allocStmt(test, hDbc, hStmt);
#endif
	for (unsigned i = 0; i < tabCount; i++) {
	    const Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    test.chk(HStmt(hStmt), found[i] == 1, "table %s not found", tab.m_name);
	}
	if (opt.m_v >= 2)
	    ndbout << "found " << cnt << " tables" << endl;
	test.run(HStmt(hStmt), SQLFreeStmt(hStmt, SQL_UNBIND));
    }
    odbc_columns: {
	unsigned found[tabCount][maxColCount];
	for (unsigned i = 0; i < tabCount; i++) {
	    for (unsigned j = 0; j < maxColCount; j++)
		found[i][j] = 0;
	}
	test.run(HStmt(hStmt), SQLColumns(hStmt, (SQLCHAR*)0, 0, (SQLCHAR*)0, 0, (SQLCHAR*)0, 0, (SQLCHAR*)0, 0));
	char tableName[200] = "";
	char columnName[200] = "";
	long dataType = 0;
	test.run(HStmt(hStmt), SQLBindCol(hStmt, 3, SQL_C_CHAR, tableName, sizeof(tableName), 0));
	test.run(HStmt(hStmt), SQLBindCol(hStmt, 4, SQL_C_CHAR, columnName, sizeof(columnName), 0));
	test.run(HStmt(hStmt), SQLBindCol(hStmt, 5, SQL_C_SLONG, &dataType, 0, 0));
	unsigned cnt = 0;
	while (1) {
	    test.exp(SQL_NO_DATA, 0, 0, false);
	    test.run(HStmt(hStmt), SQLFetch(hStmt));
	    if (test.m_ret == SQL_NO_DATA)
		break;
	    test.timerCnt(1);
	    cnt++;
	    for (unsigned i = 0; i < tabCount; i++) {
		const Tab& tab = tabList[i];
		if (! tab.optok())
		    continue;
		if (! blankeq(tab.m_name, tableName))
		    continue;
		bool columnFound = false;
		for (unsigned j = 0; j < tab.m_colCount; j++) {
		    const Col& col = tab.m_colList[j];
		    if (! blankeq(col.m_name, columnName))
			continue;
		    test.chk(HStmt(hStmt), found[i][j] == 0, "duplicate column %s.%s", tableName, columnName);
		    found[i][j]++;
		    columnFound = true;
		}
		test.chk(HStmt(hStmt), columnFound, "unknown column %s.%s", tableName, columnName);
	    }
	}
#ifndef iODBC
	test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
#else
	freeStmt(test, hDbc, hStmt);
	allocStmt(test, hDbc, hStmt);
#endif
	for (unsigned i = 0; i < tabCount; i++) {
	    const Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    for (unsigned j = 0; j < tab.m_colCount; j++) {
		const Col& col = tab.m_colList[j];
		test.chk(HStmt(hStmt), found[i][j] == 1, "column %s.%s not found", tab.m_name, col.m_name);
	    }
	}
	if (opt.m_v >= 2)
	    ndbout << "found " << cnt << " columns" << endl;
	test.run(HStmt(hStmt), SQLFreeStmt(hStmt, SQL_UNBIND));
    }
    odbc_primarykeys: {
	// table patterns are no allowed
	for (unsigned i = 0; i < tabCount; i++) {
	    const Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    char tmp[200];				// p.i.t.a
	    strcpy(tmp, tab.m_name);
	    for (char* a = tmp; *a != 0; a++) {
		if ('a' <= *a && *a <= 'z')
		    *a -= 'a' - 'A';
	    }
	    test.run(HStmt(hStmt), SQLPrimaryKeys(hStmt, (SQLCHAR*)0, 0, (SQLCHAR*)0, 0, (SQLCHAR*)tmp, SQL_NTS));
	    char tableName[200] = "";
	    char columnName[200] = "";
	    long keySeq = -1;
	    test.run(HStmt(hStmt), SQLBindCol(hStmt, 3, SQL_C_CHAR, tableName, sizeof(tableName), 0));
	    test.run(HStmt(hStmt), SQLBindCol(hStmt, 4, SQL_C_CHAR, columnName, sizeof(columnName), 0));
	    test.run(HStmt(hStmt), SQLBindCol(hStmt, 5, SQL_C_SLONG, &keySeq, 0, 0));
	    unsigned cnt = 0;
	    while (1) {
		if (cnt == tab.m_pkCount)
		    test.exp(SQL_NO_DATA, 0, 0, true);
		test.run(HStmt(hStmt), SQLFetch(hStmt));
		if (test.m_ret == SQL_NO_DATA)
		    break;
		test.chk(HStmt(hStmt), keySeq == 1 + cnt, "got %ld != %u", keySeq, 1 + cnt);
		const Col& col = tab.m_colList[tab.m_pkIndex[keySeq - 1]];
		test.chk(HStmt(hStmt), blankeq(columnName, col.m_name), "got %s != %s", columnName, col.m_name);
		test.timerCnt(1);
		cnt++;
	    }
#ifndef iODBC
	    test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
#else
	    freeStmt(test, hDbc, hStmt);
	    allocStmt(test, hDbc, hStmt);
#endif
	}
	test.run(HStmt(hStmt), SQLFreeStmt(hStmt, SQL_UNBIND));
    }
    freeAll(test, hEnv, hDbc, hStmt);
}

// insert

static void
testInsert(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	// prepare
	tab.insertAll(sqlptr = sql);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	SQLSMALLINT parCount = -1;
	test.run(HStmt(hStmt), SQLNumParams(hStmt, &parCount));
	test.chk(HStmt(hStmt), parCount == tab.m_colCount, "got %d != %d", (int)parCount, (int)tab.m_colCount);
	// bind parameters
	Row row(tab);
	for (unsigned j = 0; j < tab.m_colCount; j++) {
	    Fld& fld = row.m_fldList[j];
	    const Col& col = fld.m_col;
	    // every other at-exec
	    SQLPOINTER caddr;
	    SQLINTEGER* ind;
	    if (opt.m_noputd || j % 2 == 0) {
		caddr = fld.caddr();
		ind = fld.ind();
	    } else {
		caddr = (SQLPOINTER)j;
		ind = fld.need();
	    }
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, caddr, col.csize(), ind));
	}
	// bind columns (none)
	SQLSMALLINT colCount = -1;
	test.run(HStmt(hStmt), SQLNumResultCols(hStmt, &colCount));
	test.chk(HStmt(hStmt), colCount == 0, "got %d != 0", (int)colCount);
	// execute
	for (unsigned k = 0; k < opt.m_scale; k++) {
	    if (k % 5 == 0) {
		// rebind
		unsigned j = 0;
		Fld& fld = row.m_fldList[j];
		const Col& col = fld.m_col;
		// every other at-exec
		SQLPOINTER caddr;
		SQLINTEGER* ind;
		if (opt.m_noputd || j % 2 == 0) {
		    caddr = fld.caddr();
		    ind = fld.ind();
		} else {
		    caddr = (SQLPOINTER)j;
		    ind = fld.need();
		}
		test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, caddr, col.csize(), ind));
	    }
	    row.calcPk(test, k);
	    row.calcNk(test);
	    unsigned needData = opt.m_noputd ? 0 : tab.m_colCount / 2;
	    if (needData)
		test.exp(SQL_NEED_DATA, 0, 0, true);
	    test.run(HStmt(hStmt), SQLExecute(hStmt));
	    test.chk(HStmt(hStmt), test.m_functionCode == SQL_DIAG_INSERT, "got %d != %d", test.m_functionCode, SQL_DIAG_INSERT);
	    if (needData) {
		while (1) {
		    SQLPOINTER jPtr = (SQLPOINTER)999;
		    if (needData)
			test.exp(SQL_NEED_DATA, 0, 0, true);
		    // completes SQLExecute on success
		    test.run(HStmt(hStmt), SQLParamData(hStmt, &jPtr));
		    if (! needData)
			break;
		    unsigned j = (unsigned)jPtr;
		    test.chk(HStmt(hStmt), j < tab.m_colCount && j % 2 != 0, "got %u 0x%x", j, j);
		    Fld& fld = row.m_fldList[j];
		    const Col& col = fld.m_col;
		    SQLSMALLINT ctype = col.ctype();
		    if (k % 2 == 0 || ctype != Col::CChar)
			test.run(HStmt(hStmt), SQLPutData(hStmt, fld.caddr(), *fld.ind()));
		    else {
			// put in pieces
			unsigned size = col.csize() - 1;	// omit null terminator
			char* caddr = (char*)(fld.caddr());
			unsigned off = 0;
			while (off < size) {
			    unsigned m = size / 7;		// bytes to put
			    if (m == 0)
				m = 1;
			    if (m > size - off)
				m = size - off;
			    bool putNull = (*fld.ind() == SQL_NULL_DATA);
			    // no null terminator
			    SQLINTEGER len = putNull ? SQL_NULL_DATA : (int)m;
			    test.run(HStmt(hStmt), SQLPutData(hStmt, caddr + off, len));
			    if (putNull)
				break;
			    off += m;
			}
		    }
		    needData--;
		}
	    }
	    chkRowCount(test, hStmt, 1);
	    chkTuplesFetched(test, hStmt, 0);
	}
	test.timerCnt(opt.m_scale);
	if (opt.m_v >= 3)
	    ndbout << "inserted " << opt.m_scale <<  " into " << tab.m_name << endl;
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

// count

static void
testCount(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	long count = -1;
	selectCount(test, hStmt, tab, &count);
	test.chk(HStmt(hStmt), count == opt.m_scale * opt.m_threads, "got %ld != %u", count, opt.m_scale * opt.m_threads);
	test.timerCnt(count);
	if (opt.m_v >= 3)
	    ndbout << "counted " << (int)count <<  " rows in " << tab.m_name << endl;
    }
    // scan all at same time
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	tab.selectAll(sqlptr = sql);
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
    }
    unsigned k = 0;
    while (1) {
	for (unsigned i = 0; i < tabCount; i++) {
	    SQLHANDLE& hStmt = hStmtList[i];
	    const Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    if (k == opt.m_scale * opt.m_threads)
		test.exp(SQL_NO_DATA, 0, 0, true);
	    test.run(HStmt(hStmt), SQLFetch(hStmt));
	    if (k != opt.m_scale * opt.m_threads) {
		chkTuplesFetched(test, hStmt, k + 1);
		test.timerCnt(1);
	    } else {
		chkTuplesFetched(test, hStmt, k);
		test.exp(SQL_NO_DATA, 0, 0, true);
		test.run(HStmt(hStmt), SQLMoreResults(hStmt));
	    }
	}
	if (k == opt.m_scale * opt.m_threads)
	    break;
	k++;
    }
    if (opt.m_v >= 3)
	ndbout << "scanned " << opt.m_scale <<  " rows from each table" << endl;
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

// update

static void
testUpdatePk(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	// prepare
	tab.updatePk(sqlptr = sql);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	// bind parameters
	Row row(tab);
	SQLSMALLINT parCount = -1;
	test.run(HStmt(hStmt), SQLNumParams(hStmt, &parCount));
	test.chk(HStmt(hStmt), parCount == tab.m_colCount, "got %d != %d", (int)parCount, (int)tab.m_colCount);
	for (unsigned j = 0; j < tab.m_nkCount; j++) {
	    Fld& fld = row.m_fldList[tab.m_nkIndex[j]];
	    const Col& col = fld.m_col;
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fld.caddr(), col.csize(), fld.ind()));
	}
	for (unsigned j = 0; j < tab.m_pkCount; j++) {
	    Fld& fld = row.m_fldList[tab.m_pkIndex[j]];
	    const Col& col = fld.m_col;
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + tab.m_nkCount + j, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fld.caddr(), col.csize(), fld.ind()));
	}
	// bind columns (none)
	SQLSMALLINT colCount = -1;
	test.run(HStmt(hStmt), SQLNumResultCols(hStmt, &colCount));
	test.chk(HStmt(hStmt), colCount == 0, "got %d != 0", (int)colCount);
	// execute
	for (unsigned k = 0; k < opt.m_scale; k++) {
	    if (k % 5 == 0) {
		unsigned j = 0;
		Fld& fld = row.m_fldList[tab.m_nkIndex[j]];
		const Col& col = fld.m_col;
		test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fld.caddr(), col.csize(), fld.ind()));
	    }
	    row.calcPk(test, k);
	    row.calcNk(test);
	    test.run(HStmt(hStmt), SQLExecute(hStmt));
	    test.chk(HStmt(hStmt), test.m_functionCode == SQL_DIAG_UPDATE_WHERE, "got %d != %d", test.m_functionCode, SQL_DIAG_UPDATE_WHERE);
	    chkRowCount(test, hStmt, 1);
	    // direct update, no read has been necessary
	    chkTuplesFetched(test, hStmt, 0);
	}
	test.timerCnt(opt.m_scale);
	if (opt.m_v >= 3)
	    ndbout << "updated " << opt.m_scale <<  " in " << tab.m_name << endl;
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

static void
testUpdateScan(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	// prepare
	tab.updateRange(sqlptr = sql);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	// bind parameters
	Row row(tab);		// for set clause
	Row rowlo(tab);		// for pk ranges
	Row rowhi(tab);
	SQLSMALLINT parCount = -1;
	test.run(HStmt(hStmt), SQLNumParams(hStmt, &parCount));
	test.chk(HStmt(hStmt), parCount == tab.m_nkCount + 2 * tab.m_pkCount, "got %d != %d", (int)parCount, (int)tab.m_nkCount + 2 * (int)tab.m_pkCount);
	for (unsigned j = 0; j < tab.m_nkCount; j++) {
	    const Col& col = tab.m_colList[tab.m_nkIndex[j]];
	    Fld& fld = row.m_fldList[tab.m_nkIndex[j]];
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fld.caddr(), col.csize(), fld.ind()));
	}
	bool canInterp = true;
	for (unsigned j = 0; j < tab.m_pkCount; j++) {
	    const Col& col = tab.m_colList[tab.m_pkIndex[j]];
	    Fld& fldlo = rowlo.m_fldList[tab.m_pkIndex[j]];
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + tab.m_nkCount + 2 * j + 0, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fldlo.caddr(), col.csize(), fldlo.ind()));
	    Fld& fldhi = rowhi.m_fldList[tab.m_pkIndex[j]];
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + tab.m_nkCount + 2 * j + 1, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fldhi.caddr(), col.csize(), fldhi.ind()));
	    if (col.m_type != Col::Char)
		canInterp = false;	// XXX no unsigned yet
	}
	// execute
	row.calcPk(test, 0);
	row.calcNk(test);
	rowlo.calcPk(test, 0);
	rowhi.calcPk(test, test.m_mul);		// sucks
	test.run(HStmt(hStmt), SQLExecute(hStmt));
	test.chk(HStmt(hStmt), test.m_functionCode == SQL_DIAG_UPDATE_WHERE, "got %d != %d", test.m_functionCode, SQL_DIAG_UPDATE_WHERE);
	chkRowCount(test, hStmt, opt.m_scale);
	chkTuplesFetched(test, hStmt, canInterp ? opt.m_scale : opt.m_scale * opt.m_threads);
	test.timerCnt(opt.m_scale);
	if (opt.m_v >= 3)
	    ndbout << "updated " << opt.m_scale <<  " in " << tab.m_name << endl;
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

// verify

static void
testVerifyPk(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	// prepare
	tab.selectPk(sqlptr = sql);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	// use same row for input and output
	Row row(tab);
	// bind parameters
	SQLSMALLINT parCount = -1;
	test.run(HStmt(hStmt), SQLNumParams(hStmt, &parCount));
	test.chk(HStmt(hStmt), parCount == tab.m_pkCount, "got %d != %d", (int)parCount, (int)tab.m_pkCount);
	for (unsigned j = 0; j < tab.m_pkCount; j++) {
	    Fld& fld = row.m_fldList[tab.m_pkIndex[j]];
	    const Col& col = fld.m_col;
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fld.caddr(), col.csize(), fld.ind()));
	}
	// bind columns
	SQLSMALLINT colCount = -1;
	test.run(HStmt(hStmt), SQLNumResultCols(hStmt, &colCount));
	test.chk(HStmt(hStmt), colCount == tab.m_colCount, "got %d != %d", (int)colCount, (int)tab.m_colCount);
	for (unsigned j = 0; j < tab.m_colCount; j++) {
	    Fld& fld = row.m_fldList[j];
	    const Col& col = fld.m_col;
	    test.run(HStmt(hStmt), SQLBindCol(hStmt, 1 + j, col.ctype(), fld.caddr(), col.csize(), fld.ind()));
	}
	// row for SQLGetData
	Row rowGet(tab);
	// reference row
	Row rowRef(tab);
	// execute
	for (unsigned k = 0; k < opt.m_scale; k++) {
	    if (k % 5 == 0) {
		// rebind
		unsigned j = 0;
		Fld& fld = row.m_fldList[tab.m_pkIndex[j]];
		const Col& col = fld.m_col;
		test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fld.caddr(), col.csize(), fld.ind()));
	    }
	    row.calcPk(test, k);
	    test.run(HStmt(hStmt), SQLExecute(hStmt));
	    test.chk(HStmt(hStmt), test.m_functionCode == SQL_DIAG_SELECT_CURSOR, "got %d != %d", test.m_functionCode, SQL_DIAG_SELECT_CURSOR);
	    // fetch
	    for (unsigned k2 = 0; ; k2++) {
		if (k2 == 1)
		    test.exp(SQL_NO_DATA, 0, 0, true);
		test.run(HStmt(hStmt), SQLFetch(hStmt));
		chkTuplesFetched(test, hStmt, 1);
		if (k2 == 1)
		    break;
		rowRef.calcPk(test, k);
		test.chk(HStmt(hStmt), row.verifyPk(test, rowRef), "verify row=%d", k);
		if (test.m_const)
		    rowRef.calcPk(test, 0);
		rowRef.calcNk(test);
		test.chk(HStmt(hStmt), row.verifyNk(test, rowRef), "verify row=%d", k);
		// SQLGetData is supported independent of SQLBindCol
		if (opt.m_nogetd)
		    continue;
		for (unsigned j = 0; j < tab.m_colCount; j++) {
		    Fld& fld = rowGet.m_fldList[j];
		    fld.zero();
		    const Col& col = fld.m_col;
		    // test both variants
		    SQLSMALLINT ctype = k % 2 == 0 ? col.ctype() : SQL_ARD_TYPE;
		    if (ctype != Col::CChar)
			test.run(HStmt(hStmt), SQLGetData(hStmt, 1 + j, ctype, fld.caddr(), col.csize(), fld.ind()));
		    else {
			// get in pieces
			unsigned size = col.csize() - 1;	// omit null terminator
			char* caddr = (char*)(fld.caddr());
			unsigned off = 0;
			while (off < size) {
			    unsigned m = size / 3;		// bytes to get
			    if (m == 0)
				m = 1;
			    if (m > size - off)
				m = size - off;
			    bool getNull = (rowRef.m_fldList[j].m_ind == SQL_NULL_DATA);
			    if (off + m < size && ! getNull)
				test.exp(SQL_SUCCESS_WITH_INFO, "01004", -1, true);
			    // include null terminator in buffer size
			    test.run(HStmt(hStmt), SQLGetData(hStmt, 1 + j, ctype, caddr + off, m + 1, fld.ind()));
			    int ind = *fld.ind();
			    if (getNull) {
				test.chk(HStmt(hStmt), ind == SQL_NULL_DATA, "got %d", ind);
				break;
			    }
			    test.chk(HStmt(hStmt), ind == size - off, "got %d != %u", ind, size - off);
			    off += m;
			}
		    }
		}
		rowRef.calcPk(test, k);
		test.chk(HStmt(hStmt), rowGet.verifyPk(test, rowRef), "verify row=%d", k);
		if (test.m_const)
		    rowRef.calcPk(test, 0);
		rowRef.calcNk(test);
		test.chk(HStmt(hStmt), rowGet.verifyNk(test, rowRef), "verify row=%d", k);
		// SQLGetData again
		for (unsigned j = 0; j < tab.m_colCount; j++) {
		    Fld& fld = rowGet.m_fldList[j];
		    const Col& col = fld.m_col;
		    // test both variants
		    SQLSMALLINT ctype = k % 2 == 0 ? col.ctype() : SQL_ARD_TYPE;
		    // expect no more data
		    test.exp(SQL_NO_DATA, 0, 0, true);
		    test.run(HStmt(hStmt), SQLGetData(hStmt, 1 + j, ctype, fld.caddr(), col.csize(), fld.ind()));
		}
	    }
	    test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
	}
	test.timerCnt(opt.m_scale);
	if (opt.m_v >= 3)
	    ndbout << "verified " << opt.m_scale <<  " from " << tab.m_name << endl;
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

static void
testVerifyScan(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	// prepare
	tab.selectRange(sqlptr = sql, ! opt.m_nosort);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	// bind parameters
	Row rowlo(tab);		// use available PK fields..
	Row rowhi(tab);		// since we have no other way for now
	SQLSMALLINT parCount = -1;
	test.run(HStmt(hStmt), SQLNumParams(hStmt, &parCount));
	test.chk(HStmt(hStmt), parCount == 2 * tab.m_pkCount, "got %d != %d", (int)parCount, 2 * (int)tab.m_pkCount);
	for (unsigned j = 0; j < tab.m_pkCount; j++) {
	    const Col& col = tab.m_colList[tab.m_pkIndex[j]];
	    Fld& fldlo = rowlo.m_fldList[tab.m_pkIndex[j]];
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + 2 * j + 0, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fldlo.caddr(), col.csize(), fldlo.ind()));
	    Fld& fldhi = rowhi.m_fldList[tab.m_pkIndex[j]];
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + 2 * j + 1, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fldhi.caddr(), col.csize(), fldhi.ind()));
	}
	// bind columns
	Row row(tab);
	SQLSMALLINT colCount = -1;
	test.run(HStmt(hStmt), SQLNumResultCols(hStmt, &colCount));
	test.chk(HStmt(hStmt), colCount == tab.m_colCount, "got %d != %d", (int)colCount, (int)tab.m_colCount);
	for (unsigned j = 0; j < tab.m_colCount; j++) {
	    Fld& fld = row.m_fldList[j];
	    const Col& col = fld.m_col;
	    test.run(HStmt(hStmt), SQLBindCol(hStmt, 1 + j, col.ctype(), fld.caddr(), col.csize(), fld.ind()));
	}
	// execute
	rowlo.calcPk(test, 0);
	rowhi.calcPk(test, test.m_mul);		// sucks
	test.run(HStmt(hStmt), SQLExecute(hStmt));
	    test.chk(HStmt(hStmt), test.m_functionCode == SQL_DIAG_SELECT_CURSOR, "got %d != %d", test.m_functionCode, SQL_DIAG_SELECT_CURSOR);
	// reference row
	Row rowRef(tab);
	// fetch
	unsigned k = 0;
	SQLUINTEGER rowCount1 = (SQLUINTEGER)-1;
	test.run(HStmt(hStmt), SQLSetStmtAttr(hStmt, SQL_ATTR_ROWS_FETCHED_PTR, &rowCount1, SQL_IS_POINTER));
	while (1) {
	    unsigned countExp;
	    if (k == opt.m_scale) {
		countExp = k;
		test.exp(SQL_NO_DATA, 0, 0, true);
	    } else {
		countExp = k + 1;
	    }
	    test.run(HStmt(hStmt), SQLFetch(hStmt));
	    // let me count the ways..
	    chkRowCount(test, hStmt, countExp);
	    test.chk(HStmt(hStmt), rowCount1 == countExp, "got %lu != %u", rowCount1, countExp);
	    SQLUINTEGER rowCount2 = (SQLUINTEGER)-1;
	    test.run(HStmt(hStmt), SQLGetStmtAttr(hStmt, SQL_ATTR_ROW_NUMBER, &rowCount2, SQL_IS_POINTER, 0));
	    test.chk(HStmt(hStmt), rowCount2 == countExp, "got %lu != %u", rowCount2, countExp);
	    if (k == opt.m_scale)
		break;
	    if (! opt.m_nosort) {
		// expecting k-th row
		rowRef.calcPk(test, k);
		test.chk(HStmt(hStmt), row.verifyPk(test, rowRef), "verify row=%d", k);
		if (test.m_const)
		    rowRef.calcPk(test, 0);
		rowRef.calcNk(test);
		test.chk(HStmt(hStmt), row.verifyNk(test, rowRef), "verify row=%d", k);
	    } else {
		// expecting random row
		rowRef.copy(row);
		test.chk(HStmt(hStmt), row.verifyNk(test, rowRef), "verify row=%d", k);
	    }
	    k++;
	}
	test.timerCnt(opt.m_scale);
	if (opt.m_v >= 3)
	    ndbout << "verified " << opt.m_scale <<  " from " << tab.m_name << endl;
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

// self-join (scan followed by pk lookups)

static void
testJoin(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned cnt = opt.m_depth; cnt <= opt.m_depth; cnt++) {
	for (unsigned i = 0; i < tabCount; i++) {
	    SQLHANDLE& hStmt = hStmtList[i];
	    Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    tab.selectJoin(sqlptr = sql, cnt);
	    if (opt.m_v >= 2)
		ndbout << "SQL: " << sql << endl;
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	}
	unsigned k = 0;
	while (1) {
	    for (unsigned i = 0; i < tabCount; i++) {
		SQLHANDLE& hStmt = hStmtList[i];
		const Tab& tab = tabList[i];
		if (! tab.optok())
		    continue;
		if (k == opt.m_scale * opt.m_threads)
		    test.exp(SQL_NO_DATA, 0, 0, true);
		test.run(HStmt(hStmt), SQLFetch(hStmt));
		if (k == opt.m_scale * opt.m_threads) {
		    chkTuplesFetched(test, hStmt, k * opt.m_depth);
		    test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
		} else {
		    chkTuplesFetched(test, hStmt, (k + 1) * opt.m_depth);
		    test.timerCnt(1);
		}
	    }
	    if (k == opt.m_scale * opt.m_threads)
		break;
	    k++;
	}
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

// cartesian join (multiple nested scans)

static void
testCart(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned cnt = 2; cnt <= 2; cnt++) {
	unsigned rows = 1;
	//for (unsigned k = 0; k < opt.m_depth; k++) {
	    //rows *= opt.m_scale * opt.m_threads;
	//}
	for (unsigned i = 0; i < tabCount; i++) {
	    SQLHANDLE& hStmt = hStmtList[i];
	    Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    tab.selectCart(sqlptr = sql, cnt);
	    if (opt.m_v >= 2)
		ndbout << "SQL: " << sql << endl;
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	}
	unsigned k = 0;
	while (1) {
	    for (unsigned i = 0; i < tabCount; i++) {
		SQLHANDLE& hStmt = hStmtList[i];
		const Tab& tab = tabList[i];
		if (! tab.optok())
		    continue;
		if (k == rows)
		    test.exp(SQL_NO_DATA, 0, 0, true);
		test.run(HStmt(hStmt), SQLFetch(hStmt));
		if (k == rows) {
		    //chkTuplesFetched(test, hStmt, k);
		    test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
		} else {
		    //chkTuplesFetched(test, hStmt, k + 1);
		    test.timerCnt(1);
		}
	    }
	    if (k == rows)
		break;
	    k++;
	}
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

// delete

static void
testDeleteAll(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	long count0 = -1;
	selectCount(test, hStmt, tab, &count0);
	tab.deleteAll(sqlptr = sql);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	if (count0 == 0)
	    test.exp(SQL_NO_DATA, 0, 0, true);
	test.run(HStmt(hStmt), SQLExecute(hStmt));
#ifndef iODBC
	test.chk(HStmt(hStmt), test.m_functionCode == SQL_DIAG_DELETE_WHERE, "got %d != %d", test.m_functionCode, SQL_DIAG_DELETE_WHERE);
#endif
	SQLINTEGER rowCount = -1;
	getRowCount(test, hStmt, &rowCount);
	test.timerCnt(rowCount);
	test.chk(HStmt(hStmt), rowCount == count0, "got %d != %ld", (int)rowCount, count0);
	chkTuplesFetched(test, hStmt, rowCount);
	if (opt.m_v >= 3)
	    ndbout << "deleted " << (int)rowCount <<  " from " << tab.m_name << endl;
	long count = -1;
	selectCount(test, hStmt, tab, &count);
	test.chk(HStmt(hStmt), count == 0, "got %ld != 0", count);
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

static void
testDeletePk(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	// prepare
	tab.deletePk(sqlptr = sql);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	// bind parameters
	Row row(tab);
	SQLSMALLINT parCount = -1;
	test.run(HStmt(hStmt), SQLNumParams(hStmt, &parCount));
	test.chk(HStmt(hStmt), parCount == tab.m_pkCount, "got %d != %d", (int)parCount, (int)tab.m_colCount);
	for (unsigned j = 0; j < tab.m_pkCount; j++) {
	    Fld& fld = row.m_fldList[tab.m_pkIndex[j]];
	    const Col& col = fld.m_col;
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, col.ctype(), col.type(), col.size(), 0, fld.caddr(), col.csize(), fld.ind()));
	}
	// bind columns (none)
	SQLSMALLINT colCount = -1;
	test.run(HStmt(hStmt), SQLNumResultCols(hStmt, &colCount));
	test.chk(HStmt(hStmt), colCount == 0, "got %d != 0", (int)colCount);
	// execute
	for (unsigned k = 0; k < opt.m_scale; k++) {
	    row.calcPk(test, k);
	    test.run(HStmt(hStmt), SQLExecute(hStmt));
	    test.chk(HStmt(hStmt), test.m_functionCode == SQL_DIAG_DELETE_WHERE, "got %d != %d", test.m_functionCode, SQL_DIAG_DELETE_WHERE);
	    chkRowCount(test, hStmt, 1);
	    // direct delete, no fetch required
	    chkTuplesFetched(test, hStmt, 0);
	}
	test.timerCnt(opt.m_scale);
	if (opt.m_v >= 3)
	    ndbout << "updated " << opt.m_scale <<  " in " << tab.m_name << endl;
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

static void
testTrans(Test& test)
{
#ifdef unixODBC
    if (opt.m_v >= 1)
	ndbout << "unixODBC does not support transactions - test skipped" << endl;
#else
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    // delete all
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	tab.deleteAll(sqlptr = sql);
	test.exp(SQL_NO_DATA, 0, 0, false);
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	SQLINTEGER rowCount = -1;
	getRowCount(test, hStmt, &rowCount);
	if (opt.m_v >= 3)
	    ndbout << "deleted " << (int)rowCount <<  " from " << tab.m_name << endl;
    }
    setAutocommit(test, hDbc, false);
    if (opt.m_v >= 2)
	ndbout << "set autocommit OFF" << endl;
    for (int commit = 0; commit < opt.m_scale; commit += 1) {
	bool rollback = (commit % 2 == 0);
	// XXX delete with no data leaves trans in error state for 2nd table
	if (commit > 0 && rollback) {	// previous case was commit
	    for (unsigned i = 0; i < tabCount; i++) {
		SQLHANDLE& hStmt = hStmtList[i];
		const Tab& tab = tabList[i];
		if (! tab.optok())
		    continue;
		tab.deleteDirect(sqlptr = sql, 0);
		if (opt.m_v >= 2)
		    ndbout << "SQL: " << sql << endl;
		test.exp(SQL_NO_DATA, 0, 0, false);
		test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    }
	    test.run(HDbc(hDbc), SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_COMMIT));
	}
	// insert
	for (unsigned i = 0; i < tabCount; i++) {
	    SQLHANDLE& hStmt = hStmtList[i];
	    const Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    tab.insertDirect(sqlptr = sql, 0);
	    if (opt.m_v >= 2)
		ndbout << "SQL: " << sql << endl;
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    if (opt.m_v >= 2)
		ndbout << tab.m_name << ": inserted 1 row" << endl;
	}
	// count them via pk
	for (unsigned i = 0; i < tabCount; i++) {
	    SQLHANDLE& hStmt = hStmtList[i];
	    const Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    tab.countDirect(sqlptr = sql, 0);
	    long count = -1;
	    long countExp = 1;
	    selectCount(test, hStmt, sql, &count);
	    test.chk(HStmt(hStmt), count == countExp, "got %ld != %ld", count, countExp);
	}
	// count them via scan
	for (unsigned i = 0; i < tabCount; i++) {
	    // XXX hupp no work
	    break;
	    SQLHANDLE& hStmt = hStmtList[i];
	    const Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    long count = -1;
	    long countExp = 1;
	    selectCount(test, hStmt, tab, &count);
	    test.chk(HStmt(hStmt), count == countExp, "got %ld != %ld", count, countExp);
	}
	// rollback or commit
	if (rollback) {
	    if (opt.m_v >= 2)
		ndbout << "end trans ROLLBACK" << endl;
	    test.run(HDbc(hDbc), SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_ROLLBACK));
	} else {
	    if (opt.m_v >= 2)
		ndbout << "end trans COMMIT" << endl;
	    test.run(HDbc(hDbc), SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_COMMIT));
	}
	// count them via pk again
	for (unsigned i = 0; i < tabCount; i++) {
	    SQLHANDLE& hStmt = hStmtList[i];
	    const Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    tab.countDirect(sqlptr = sql, 0);
	    long count = -1;
	    long countExp = rollback ? 0 : 1;
	    selectCount(test, hStmt, sql, &count);
	    test.chk(HStmt(hStmt), count == countExp, "got %ld != %ld", count, countExp);
	}
	// count them via scan again
	for (unsigned i = 0; i < tabCount; i++) {
	    // XXX hupp no work
	    break;
	    SQLHANDLE& hStmt = hStmtList[i];
	    const Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    long count = -1;
	    long countExp = rollback ? 0 : 1;
	    selectCount(test, hStmt, tab, &count);
	    test.chk(HStmt(hStmt), count == countExp, "got %ld != %ld", count, countExp);
	}
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
#endif
}

static void
testConcur(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmtList[tabCount];
    allocAll(test, hEnv, hDbc, hStmtList, tabCount);
    char sql[MAX_SQL], *sqlptr;
    for (unsigned i = 0; i < tabCount; i++) {
	SQLHANDLE& hStmt = hStmtList[i];
	const Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	// delete all
	tab.deleteAll(sqlptr = sql);
	test.exp(SQL_NO_DATA, 0, 0, false);
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	// insert some
	unsigned rowcount = 10;
	for (unsigned n = 0; n < rowcount; n++) {
	    tab.insertDirect(sqlptr = sql, n);
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	}
	verifyCount(test, hStmt, tab, rowcount);
	// start query scan followed by pk lookups
	tab.selectJoin(sqlptr = sql, 2);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	// start fetch
	unsigned k = 0;
	while (1) {
	    if (k > 0)
		test.exp(SQL_ERROR, "24000", -1, true);	// commit closed cursor
	    test.run(HStmt(hStmt), SQLFetch(hStmt));
	    if (k > 0)
		break;
	    // delete some random row
	    tab.deleteDirect(sqlptr = sql, k);
	    // try using same statement
	    test.exp(SQL_ERROR, "24000", -1, true);	// cursor is open
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    // try using different statement
	    SQLHANDLE hStmt2;
	    allocStmt(test, hDbc, hStmt2);
	    test.run(HStmt(hStmt2), SQLExecDirect(hStmt2, (SQLCHAR*)sql, SQL_NTS));
	    k++;
	}
	test.exp(SQL_ERROR, "24000", -1, true);		// cursor is not open
	test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
	test.timerCnt(rowcount);
    }
    freeAll(test, hEnv, hDbc, hStmtList, tabCount);
}

static void
testReadcom(Test& test)
{
    testDeleteAll(test);
    testInsert(test);
    const unsigned nc = 3;
    SQLHANDLE hEnv[nc], hDbc[nc], hStmt[nc];
    char sql[MAX_SQL], *sqlptr;
    for (unsigned j = 0; j < nc; j++)
	allocAll(test, hEnv[j], hDbc[j], hStmt[j]);
    for (unsigned i = 0; i < tabCount; i++) {
	Tab& tab = tabList[i];
	if (! tab.optok())
	    continue;
	long count;
	// check count
	count = -1;
	selectCount(test, hStmt[0], tab, &count);
	test.chk(HStmt(hStmt[0]), count == opt.m_scale, "got %d != %d", (int)count, (int)opt.m_scale);
	// scan delete uncommitted with handle 0
	setAutocommit(test, hDbc[0], false);
	tab.deleteAll(sqlptr = sql);
	if (opt.m_scale == 0)
	    test.exp(SQL_NO_DATA, 0, 0, false);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt[0]), SQLExecDirect(hStmt[0], (SQLCHAR*)sql, SQL_NTS));
	// scan via other tx should not hang and see all rows
	for (unsigned j = 0; j < nc; j++) {
	    count = -1;
	    int want = j == 0 ? 0 : opt.m_scale;
	    selectCount(test, hStmt[j], tab, &count);
	    test.chk(HStmt(hStmt[j]), count == want, "tx %u: got %d != %d", j, (int)count, want);
	    if (opt.m_v >= 2)
		ndbout << "tx " << j << " ok !" << endl;
	}
	// setting autocommit on commits the delete
	setAutocommit(test, hDbc[0], true);
	// check count
	count = -1;
	selectCount(test, hStmt[0], tab, &count);
	test.chk(HStmt(hStmt[0]), count == 0, "got %d != 0", (int)count);
    }
    for (unsigned j = 0; j < nc; j++)
	freeAll(test, hEnv[j], hDbc[j], hStmt[j]);
}

static void
testPerf(Test& test)
{
    if (test.m_stuff == 0) {
	SQLHANDLE hEnv, hDbc, hStmt;
	allocAll(test, hEnv, hDbc, hStmt);
	char sql[MAX_SQL], *sqlptr;
	for (unsigned i = 0; i < tabCount; i++) {
	    Tab& tab = tabList[i];
	    if (! tab.optok())
		continue;
	    test.exp(SQL_NO_DATA, 0, 0, false);
	    tab.deleteAll(sqlptr = sql);
	    if (opt.m_v >= 2)
		ndbout << "SQL: " << sql << endl;
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    long count0 = -1;
	    // XXX triggers SEGV somewhere
	    //selectCount(test, hStmt, tab, &count0);
	    //test.chk(HStmt(hStmt), count0 == 0, "got %d != 0", (int)count0);
	}
	freeAll(test, hEnv, hDbc, hStmt);
	return;
    }
    assert(test.m_stuff == 1 || test.m_stuff == 2);
    bool ndbapi = (test.m_stuff == 1);
    tt01: {
	const unsigned OFF = 1000000;
	const unsigned N = 25;
	Tab& tab = tabList[1];
	if (! tab.optok())
	    goto out;
	if (ndbapi) {
#ifndef ndbODBC
	    if (opt.m_v >= 1)
		ndbout << "running via DM - test skipped" << endl;
#else
	    Ndb* ndb = new Ndb("TEST_DB");
	    ndb->init();
	    if (ndb->waitUntilReady() != 0) {
		ndbout << ndb->getNdbError() << endl;
		fatal("waitUntilReady");
	    }
	    Uint32 val[1+N];
	    // insert
	    for (unsigned k = 1; k <= opt.m_scale; k++) {
		NdbConnection* con = ndb->startTransaction();
		if (con == 0) {
		    ndbout << ndb->getNdbError() << endl;
		    fatal("startTransaction");
		}
		NdbOperation* op = con->getNdbOperation(tab.m_upperName);
		if (op == 0) {
		    ndbout << con->getNdbError() << endl;
		    fatal("getNdbOperation");
		}
		if (op->insertTuple() == -1) {
		    ndbout << op->getNdbError() << endl;
		    fatal("insertTuple");
		}
		for (unsigned j = 0; j <= N; j++) {
		    val[j] = (j == 0 ? k + test.m_no * OFF : k * j);
		    if (j == 0) {
			if (op->equal(j, val[j]) == -1) {
			    ndbout << op->getNdbError() << endl;
			    fatal("equal");
			}
		    } else {
			if (op->setValue(j, val[j]) == -1) {
			    ndbout << op->getNdbError() << endl;
			    fatal("setValue");
			}
		    }
		}
		if (con->execute(Commit) == -1) {
		    ndbout << con->getNdbError() << endl;
		    fatal("execute");
		}
		ndb->closeTransaction(con);
	    }
	    test.timerCnt(opt.m_scale);
	    // select PK
	    for (unsigned k = 1; k <= opt.m_scale; k++) {
		NdbConnection* con = ndb->startTransaction();
		if (con == 0) {
		    ndbout << ndb->getNdbError() << endl;
		    fatal("startTransaction");
		}
		NdbOperation* op = con->getNdbOperation(tab.m_upperName);
		if (op == 0) {
		    ndbout << con->getNdbError() << endl;
		    fatal("getNdbOperation");
		}
		if (op->readTuple() == -1) {
		    ndbout << op->getNdbError() << endl;
		    fatal("insertTuple");
		}
		for (unsigned j = 0; j <= N; j++) {
		    val[j] = (j == 0 ? k + test.m_no * OFF : 0);
		    if (j == 0) {
			if (op->equal(j, val[j]) == -1) {
			    ndbout << op->getNdbError() << endl;
			    fatal("equal");
			}
		    } else {
			if (op->getValue(j, (char*)&val[j]) == 0) {
			    ndbout << op->getNdbError() << endl;
			    fatal("getValue");
			}
		    }
		}
		if (con->execute(Commit) == -1) {
		    ndbout << con->getNdbError() << endl;
		    fatal("execute");
		}
		for (unsigned j = 1; j <= N; j++) {
		    assert(val[j] == k * j);
		}
		ndb->closeTransaction(con);
	    }
	    test.timerCnt(opt.m_scale);
	    // delete PK
	    for (unsigned k = 1; k <= opt.m_scale; k++) {
		NdbConnection* con = ndb->startTransaction();
		if (con == 0) {
		    ndbout << ndb->getNdbError() << endl;
		    fatal("startTransaction");
		}
		NdbOperation* op = con->getNdbOperation(tab.m_upperName);
		if (op == 0) {
		    ndbout << con->getNdbError() << endl;
		    fatal("getNdbOperation");
		}
		if (op->deleteTuple() == -1) {
		    ndbout << op->getNdbError() << endl;
		    fatal("deleteTuple");
		}
		unsigned j = 0;
		val[j] = k + test.m_no * OFF;
		if (op->equal(j, val[j]) == -1) {
		    ndbout << op->getNdbError() << endl;
		    fatal("equal");
		}
		if (con->execute(Commit) == -1) {
		    ndbout << con->getNdbError() << endl;
		    fatal("execute");
		}
		ndb->closeTransaction(con);
	    }
	    test.timerCnt(opt.m_scale);
	    delete ndb;
#endif
	} else {
	    SQLHANDLE hEnv, hDbc, hStmt;
	    allocAll(test, hEnv, hDbc, hStmt);
	    long val[1+N];
	    char sql[MAX_SQL], *sqlptr;
	    // insert
	    tab.insertAll(sqlptr = sql);
	    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    for (unsigned j = 0; j <= N; j++) {
		test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &val[j], 0, 0));
	    }
	    test.m_perf = true;
	    for (unsigned k = 1; k <= opt.m_scale; k++) {
		for (unsigned j = 0; j <= N; j++) {
		    val[j] = (j == 0 ? k + test.m_no * OFF : k * j);
		}
		test.run(HStmt(hStmt), SQLExecute(hStmt));
	    }
	    test.m_perf = false;
	    test.timerCnt(opt.m_scale);
	    // select PK
	    tab.selectPk(sqlptr = sql);
	    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    for (unsigned j = 0; j <= N; j++) {
		test.run(HStmt(hStmt), SQLBindCol(hStmt, 1 + j, SQL_C_SLONG, &val[j], 0, 0));
	    }
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + N + 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &val[0], 0, 0));
	    test.m_perf = true;
	    for (unsigned k = 1; k <= opt.m_scale; k++) {
		val[0] = k + test.m_no * OFF;
		test.run(HStmt(hStmt), SQLExecute(hStmt));
		test.run(HStmt(hStmt), SQLFetch(hStmt));
		for (unsigned j = 1; j <= N; j++) {
		    assert(val[j] == k * j);
		}
		test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
	    }
	    test.m_perf = false;
	    test.timerCnt(opt.m_scale);
	    // delete PK
	    tab.deletePk(sqlptr = sql);
	    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    unsigned j = 0;
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1 + j, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &val[j], 0, 0));
	    test.m_perf = true;
	    for (unsigned k = 1; k <= opt.m_scale; k++) {
		val[j] = k + test.m_no * OFF;
		test.run(HStmt(hStmt), SQLExecute(hStmt));
	    }
	    test.m_perf = false;
	    test.timerCnt(opt.m_scale);
	    freeAll(test, hEnv, hDbc, hStmt);
	}
    out:
    	;
    }
}

struct Sql {
    const char* m_sql;
    int m_functionCode;
    int m_rowCount;
    int m_tuplesFetched;
    long m_lastValue;
    unsigned long m_bindValue;
    int m_ret;
    const char* m_state;
    SQLINTEGER m_native;
    bool m_reset;
    // run this function instead
    typedef void (*TestFunc)(Test& test);
    TestFunc m_testFunc;
    Sql() :
	m_sql(0) {
    }
    Sql(const char* do_cmd) :
	m_sql(do_cmd) {
    }
    Sql(const char* sql, int functionCode, int rowCount, int tuplesFetched, long lastValue, long bindValue) :
        m_sql(sql),
	m_functionCode(functionCode),
	m_rowCount(rowCount),
	m_tuplesFetched(tuplesFetched),
	m_lastValue(lastValue),
	m_bindValue(bindValue),
	m_ret(SQL_SUCCESS),
	m_state(0),
	m_native(0),
	m_reset(true),
	m_testFunc(0) {
    }
    // the 4 numbers after SQL_DIAG... rowCount tuplesFetched lastValue bindValue
    Sql(const char* sql, int functionCode, int rowCount, int tuplesFetched, long lastValue, long bindValue, int ret, const char* state, SQLINTEGER native, bool reset) :
        m_sql(sql),
	m_functionCode(functionCode),
	m_rowCount(rowCount),
	m_tuplesFetched(tuplesFetched),
	m_lastValue(lastValue),
	m_bindValue(bindValue),
	m_ret(ret),
	m_state(state),
	m_native(native),
	m_reset(reset),
	m_testFunc(0) {
    }
    Sql(const char* text, TestFunc testFunc) :
	m_sql(text),
	m_testFunc(testFunc) {
    }
    static const char* set_autocommit_on() {
	return "set autocommit on";
    }
    static const char* set_autocommit_off() {
	return "set autocommit off";
    }
    static const char* do_commit() {
	return "commit";
    }
    static const char* do_rollback() {
	return "rollback";
    }
};

// 90

static const Sql
miscSql90[] = {
    Sql("select * from dual",
	SQL_DIAG_SELECT_CURSOR, 1, 0, -1, -1),
    Sql("drop table tt90a",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table tt90a (a int, b int, c int, primary key(b, c)) storage(large) logging",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    Sql()
};

// 91

static const Sql
miscSql91[] = {
    Sql("drop table tt91a",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table tt91a (a bigint unsigned primary key, b bigint unsigned  not null, c varchar(10))",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    Sql("insert into tt91a values (1, 111, 'aaa')",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    // fails
    Sql("insert into tt91a values (2, null, 'ccc')",
	SQL_DIAG_INSERT, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2014203, true),
    Sql("update tt91a set b = 222 where a = 2",
	SQL_DIAG_UPDATE_WHERE, 0, 0, -1, -1,
	SQL_NO_DATA, 0, 0, true),
    // two more
    Sql("insert into tt91a values (2, 222, 'ccc')",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    Sql("insert into tt91a values (3, 333, 'bbb')",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    // direct update
    Sql("update tt91a set b = 112 where a = 1",
	SQL_DIAG_UPDATE_WHERE, 1, 0, -1, -1),
    Sql("update tt91a set b = 113 where a = 1 and b > 111",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    // update and delete with interpreted scan
    Sql("update tt91a set b = 114 where b < 114",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    Sql("delete from tt91a where b < 115",
	SQL_DIAG_DELETE_WHERE, 1, 1, -1, -1),
    Sql("insert into tt91a values (1, 111, 'aaa')",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    // check rows: 1,111,aaa + 2,222,ccc + 3,333,bbb
    Sql("select * from tt91a order by c",
	SQL_DIAG_SELECT_CURSOR, 3, 3, 2, -1),
    Sql("select * from tt91a order by c desc",
	SQL_DIAG_SELECT_CURSOR, 3, 3, 1, -1),
    Sql("select * from tt91a where a = 2",
	SQL_DIAG_SELECT_CURSOR, 1, 1, -1, -1),
    Sql("select * from tt91a where a + b = 224",
	SQL_DIAG_SELECT_CURSOR, 1, 3, -1, -1),
    Sql("select * from tt91a where a = 4",
	SQL_DIAG_SELECT_CURSOR, 0, 0, -1, -1),
    Sql("select b-a from tt91a order by a-b",
	SQL_DIAG_SELECT_CURSOR, 3, 3, 110, -1),
    Sql("select sum(a+b) from tt91a",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 672, -1),
    Sql("select x.b, y.b, z.b from tt91a x, tt91a y, tt91a z where x.b <= y.b and y.b < z.b order by x.b",
	SQL_DIAG_SELECT_CURSOR, 4, 13, 222, -1),
    Sql("select x.b, y.b, z.b from tt91a x, tt91a y, tt91a z where x.b + y.b = z.b order by x.b",
	SQL_DIAG_SELECT_CURSOR, 3, 15, 222, -1),
    // tmp index
    Sql("create unique hash index xx91a on tt91a(b)",
	SQL_DIAG_CREATE_INDEX, -1, -1, -1, -1),
    Sql("select x.b, y.b, z.b from tt91a x, tt91a y, tt91a z where x.b + y.b = z.b order by x.b",
	SQL_DIAG_SELECT_CURSOR, 3, 15, 222, -1),
    Sql("drop index xx91a on tt91a",
	SQL_DIAG_DROP_INDEX, -1, -1, -1, -1),
    // add some duplicates
    Sql("insert into tt91a values (4, 222, 'ccc')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("insert into tt91a values (5, 333, 'bbb')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("insert into tt91a values (6, 333, 'bbb')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    // check rows: 1,111,aaa + 2 * 2,222,ccc  + 3 * 3,333,bbb
    Sql("select count(*) from tt91a",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 6, -1),
    Sql("select a+b from tt91a where (b = 111 or b = 222 ) and (b = 222 or b = 333) and a > 1 and a < 3",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 224, -1),
    Sql("select sum(a) from tt91a having min(a) = 1 and max(a) = 6",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 21, -1),
    Sql("select sum(a) from tt91a where a = 2 or a = 4 having min(a) = 2 and max(a) = 4",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 6, -1),
    Sql("select sum(a) from tt91a having min(a) = 1 and max(a) = 5",
	SQL_DIAG_SELECT_CURSOR, 0, -1, -1, -1),
    Sql("select sum(a), b from tt91a group by b order by b",
	SQL_DIAG_SELECT_CURSOR, 3, -1, 14, -1),
    Sql("select sum(a), b, c from tt91a group by b, c order by c",
	SQL_DIAG_SELECT_CURSOR, 3, -1, 6, -1),
    Sql("select b, sum(a) from tt91a group by b having b = 37 * sum(a)",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 222, -1),
    // simple varchar vs interpreter test
    Sql("select count(*) from tt91a where c = 'ccc'",
	SQL_DIAG_SELECT_CURSOR, 1, 2, 2, -1),
    Sql("select count(*) from tt91a where c like '%b%'",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 3, -1),
    // interpreter limits (crashes in api on v211)
#if NDB_VERSION_MAJOR >= 3
    Sql("select count(*) from tt91a where a in (99,98,97,96,95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,99,98,97,96,95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,99,98,97,96,95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,99,98,97,96,95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,99,98,97,96,95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,99,98,97,96,95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2)",
	SQL_DIAG_SELECT_CURSOR, 1, 5, 5, -1),
    Sql("select count(*) from tt91a where c in ('xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','bbb','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy','zzzzz','xxxxx','yyyyy')",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 3, -1),
#endif
    // distinct
    Sql("select distinct b from tt91a order by b",
	SQL_DIAG_SELECT_CURSOR, 3, -1, 333, -1),
    // some illegal groupings
    Sql("select a from tt91a group by b",
	-1, -1, -1, -1, -1,
	SQL_ERROR, "IM000", -1, -1),
    Sql("select sum(a) from tt91a group by b having a = 2",
	-1, -1, -1, -1, -1,
	SQL_ERROR, "IM000", -1, -1),
    Sql("select sum(a) from tt91a group by b order by a",
	-1, -1, -1, -1, -1,
	SQL_ERROR, "IM000", -1, -1),
    // string functions
    Sql("insert into tt91a (c, b, a) values ('abcdef', 999, 9)",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("select count(*) from tt91a where left(c, 2) = 'ab' and substr(c, 3, 2) = 'cd' and right(c, 2) = 'ef'",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 1, -1),
    // nulls
    Sql("update tt91a set c = null where a > 8",
	SQL_DIAG_UPDATE_WHERE, 1, -1, -1, -1),
    Sql("select a from tt91a where c is null and b is not null order by a",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 9, -1),
    Sql("select a from tt91a where not (c is not null or b is null) order by a",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 9, -1),
    // null value guard in interpreter
    Sql("select count(*) from tt91a where c < 'x' or c > 'x' or c != 'x' or c = 'x'",
	SQL_DIAG_SELECT_CURSOR, 1, 6, 6, -1),
    Sql("delete from tt91a where c is null",
	SQL_DIAG_DELETE_WHERE, 1, -1, -1, -1),
    // indexes
    Sql("update tt91a set b = a + 5",
	SQL_DIAG_UPDATE_WHERE, 6, 6, -1, -1),
    Sql("create unique hash index xx91a on tt91a(b)",
	SQL_DIAG_CREATE_INDEX, -1, -1, -1, -1),
    // scan y primary key x
    Sql("select x.b from tt91a x, tt91a y where x.a = y.b + 0",
	SQL_DIAG_SELECT_CURSOR, 1, 7, 11, -1),
    // scan x index y
    Sql("select x.b from tt91a x, tt91a y where x.a + 0 = y.b",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 11, -1),
    // scan x scan y
    Sql("select x.b from tt91a x, tt91a y where x.a + 0 = y.b + 0",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 11, -1),
    // dml ops
    Sql("delete from tt91a where b = 11 and a > 999",
	SQL_DIAG_DELETE_WHERE, 0, 1, -1, -1,
	SQL_NO_DATA, 0, 0, true),
    Sql("delete from tt91a where b = 11",
	SQL_DIAG_DELETE_WHERE, 1, 0, -1, -1),
    Sql("delete from tt91a where b = 11",
	SQL_DIAG_DELETE_WHERE, 0, 0, -1, -1,
	SQL_NO_DATA, 0, 0, true),
    Sql("update tt91a set b = 10*10 where b = 10",
	SQL_DIAG_UPDATE_WHERE, 1, 0, -1, -1),
    Sql("update tt91a set b = 10 where b = 10*10",
	SQL_DIAG_UPDATE_WHERE, 1, 0, -1, -1),
    Sql("update tt91a set b = 10*10 where b = 10 and b >= 10",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    Sql("update tt91a set b = 10 where b = 10*10 and b >= 10*10",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    // char vs varchar
    Sql("drop table tt91b",
	SQL_DIAG_DROP_TABLE, -1, -1, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table tt91b (a int primary key, b char(5), c varchar(5))",
	SQL_DIAG_CREATE_TABLE, -1, -1, -1, -1),
    Sql("insert into tt91b values (1, 'abc', 'abc')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("insert into tt91b values (2, 'xyz', 'xyz')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("insert into tt91b values (3, 'xyz', 'xyz  ')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    // char = char strips blanks
    Sql("select count(*) from tt91b x where (x.b = 'abc') or x.a = x.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 1, -1),
    Sql("select count(*) from tt91b x where (x.b = 'abc')",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql("select count(*) from tt91b x where (x.b = 'abc       ') or x.a = x.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 1, -1),
    Sql("select count(*) from tt91b x where (x.b = 'abc       ')",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    // varchar = char
    Sql("select count(*) from tt91b x where (x.c = 'abc') or x.a = x.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 1, -1),
    Sql("select count(*) from tt91b x where (x.c = 'abc')",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql("select count(*) from tt91b x where (x.c = 'abc ') or x.a = x.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 0, -1),
    Sql("select count(*) from tt91b x where (x.c = 'abc ')",
	SQL_DIAG_SELECT_CURSOR, 1, 0, 0, -1),
    // char = varchar
    Sql("select count(*) from tt91b x, tt91b y where (x.b = y.c) or x.a = x.a+1 or y.a = y.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 2, -1),
    Sql("select count(*) from tt91b x, tt91b y where (x.b = y.c)",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 2, -1),
    // varchar = varchar
    Sql("select count(*) from tt91b x, tt91b y where (x.c = y.c) or x.a = x.a+1 or y.a = y.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 3, -1),
    Sql("select count(*) from tt91b x, tt91b y where (x.c = y.c)",
	SQL_DIAG_SELECT_CURSOR, 1, -1, 3, -1),
    // less
    Sql("select 10 * x.a + y.a from tt91b x, tt91b y where (x.b < y.b) or x.a = x.a+1 or y.a = y.a+1 order by x.a, y.a",
	SQL_DIAG_SELECT_CURSOR, 2, -1, 13, -1),
    Sql("select 10 * x.a + y.a from tt91b x, tt91b y where (x.b < y.b) order by x.a, y.a",
	SQL_DIAG_SELECT_CURSOR, 2, -1, 13, -1),
    Sql("select 10 * x.a + y.a from tt91b x, tt91b y where (x.c < y.c) or x.a = x.a+1 or y.a = y.a+1 order by x.a, y.a",
	SQL_DIAG_SELECT_CURSOR, 3, -1, 23, -1),
    Sql("select 10 * x.a + y.a from tt91b x, tt91b y where (x.c < y.c) order by x.a, y.a",
	SQL_DIAG_SELECT_CURSOR, 3, -1, 23, -1),
    // like
    Sql("select count(*) from tt91b x where (x.b like 'a%') or x.a = x.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 1, -1),
    Sql("select count(*) from tt91b x where (x.b like 'a%')",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql("select count(*) from tt91b x where (x.b like 'x%z') or x.a = x.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 0, -1),
    Sql("select count(*) from tt91b x where (x.b like 'x%z')",
	SQL_DIAG_SELECT_CURSOR, 1, 0, 0, -1),
    Sql("select count(*) from tt91b x where (x.a+0 = 2 and x.c like 'x%z') or x.a = x.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 1, -1),
    Sql("select count(*) from tt91b x where (x.a+0 = 2 and x.c like 'x%z')",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql("select count(*) from tt91b x where (x.a+0 = 3 and x.c like 'x%z  ') or x.a = x.a+1",
	SQL_DIAG_SELECT_CURSOR, 1, 3, 1, -1),
    Sql("select count(*) from tt91b x where (x.a+0 = 3 and x.c like 'x%z  ')",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql()
};

// 92

static void
testMisc92a(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    char sql[MAX_SQL];
    char tname[20];
    sprintf(tname, "tt92%c", 0140 + test.m_no);
    if (test.m_loop == 1) {
	lock_mutex();
	sprintf(sql, "drop table %s", tname);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.exp(SQL_ERROR, "IM000", 2040709, false);
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	sprintf(sql, "create table %s (a int unsigned primary key, b int unsigned not null)", tname);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	unlock_mutex();
    } else {
	sprintf(sql, "delete from %s", tname);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.exp(SQL_NO_DATA, 0, 0, false);
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	test.run(HStmt(hStmt), SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_COMMIT));
    }
    for (int on = true; on >= false; on--) {
	if (opt.m_v >= 2)
	    ndbout << "set autocommit " << (on ? "ON" : "OFF") << endl;
	setAutocommit(test, hDbc, on);
	// insert rows
	if (opt.m_v >= 2)
	    ndbout << "SQL: insert into " << tname << " ..." << opt.m_scale << endl;
	for (unsigned k = 0; k < opt.m_scale; k++) {
	    sprintf(sql, "insert into %s values (%u, %u)", tname, k, 10 * k);
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	}
	// commit always
	test.run(HStmt(hStmt), SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_COMMIT));
	// scan delete
	sprintf(sql, "delete from %s", tname);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	// rollback or commit
	test.run(HStmt(hStmt), SQLEndTran(SQL_HANDLE_DBC, hDbc, on ? SQL_COMMIT : SQL_ROLLBACK));
	// count
	long count = -1;
	sprintf(sql, "select count(*) from %s", tname);
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	selectCount(test, hStmt, sql, &count);
	test.chk(HStmt(hStmt), count == on ? 0 : opt.m_scale, "%s: got %d != %d", tname, (int)count, (int)opt.m_scale);
    }
    freeAll(test, hEnv, hDbc, hStmt);
}

static const Sql
miscSql92[] = {
    // create in C func
    Sql("testMisc92a", testMisc92a),
    Sql()
};

// 93

static void
testMisc93a(Test& test)
{
    SQLHANDLE hEnv[2], hDbc[2], hStmt[2];
    allocAll(test, hEnv[0], hDbc[0], hStmt[0]);
    allocAll(test, hEnv[1], hDbc[1], hStmt[1]);
    char sql[MAX_SQL];
    // select via primary key
    setAutocommit(test, hDbc[0], false);
    sprintf(sql, "select c1 from tt93a where c0 = 1");
    if (opt.m_v >= 2)
	ndbout << "SQL: " << sql << endl;
    test.run(HStmt(hStmt[0]), SQLExecDirect(hStmt[0], (SQLCHAR*)sql, SQL_NTS));
    // update via another trans must time out
    sprintf(sql, "update tt93a set c1 = 'b' where c0 = 1");
    if (opt.m_v >= 2)
	ndbout << "SQL: " << sql << endl;
    test.run(HStmt(hStmt[1]), SQLExecDirect(hStmt[1], (SQLCHAR*)sql, SQL_NTS));
    freeAll(test, hEnv[0], hDbc[0], hStmt[0]);
    freeAll(test, hEnv[1], hDbc[1], hStmt[1]);
}

static const Sql
miscSql93[] = {
    // create in C func
    Sql("drop table tt93a",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table tt93a (c0 int primary key, c1 char(10))",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    Sql("insert into tt93a values(1, 'a')",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    Sql("testMisc93a", testMisc93a),
    Sql()
};

// 95

static const Sql
miscSql95[] = {
    Sql("drop table tt95a",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table tt95a (a int not null, b char(10) not null, c int not null, d char(10), primary key(a, b)) storage(small)",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    // ordered index create and drop
    Sql("create index xx95a on tt95a (c, d) nologging",
	SQL_DIAG_CREATE_INDEX, -1, -1, -1, -1),
    Sql("drop index xx95a on tt95a",
	SQL_DIAG_DROP_INDEX, -1, -1, -1, -1),
    Sql("create index xx95a on tt95a (c) nologging",
	SQL_DIAG_CREATE_INDEX, -1, -1, -1, -1),
    Sql("insert into tt95a values(1, 'a', 10, 'b')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("insert into tt95a values(2, 'a', 20, 'b')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("insert into tt95a values(3, 'a', 30, 'b')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("select a from tt95a where c = 20",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 2, -1),
    Sql("delete from tt95a where c = 10",
	SQL_DIAG_DELETE_WHERE, 1, 1, -1, -1),
    Sql("update tt95a set c = 300 where c = 30",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    Sql("delete from tt95a where c = 300",
	SQL_DIAG_DELETE_WHERE, 1, 1, -1, -1),
    Sql("delete from tt95a",
	SQL_DIAG_DELETE_WHERE, 1, 1, -1, -1),
    // simple insert and rollback
    Sql("-- simple insert and rollback"),
    Sql(Sql::set_autocommit_off()),
    Sql("insert into tt95a values(1, 'a', 10, 'b')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("select count(*) from tt95a",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql("select count(*) from tt95a where c = 10",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql(Sql::do_rollback()),
    Sql(Sql::set_autocommit_on()),
    Sql("select count(*) from tt95a",
	SQL_DIAG_SELECT_CURSOR, 1, 0, 0, -1),
    // simple update and rollback
    Sql("-- simple update and rollback"),
    Sql("insert into tt95a values(1, 'a', 10, 'b')",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql(Sql::set_autocommit_off()),
    Sql("update tt95a set c = 20 where c = 10",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    Sql("select count(*) from tt95a",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql("select count(*) from tt95a where c = 20",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql(Sql::do_rollback()),
    Sql(Sql::set_autocommit_on()),
    Sql("select count(*) from tt95a where c = 10",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    // simple delete and rollback
    Sql("-- simple delete and rollback"),
    Sql(Sql::set_autocommit_off()),
    Sql("delete from tt95a where c = 10",
	SQL_DIAG_DELETE_WHERE, 1, 1, -1, -1),
    Sql("select count(*) from tt95a",
	SQL_DIAG_SELECT_CURSOR, 0, 0, 0, -1),
    Sql("select count(*) from tt95a where c = 10",
	SQL_DIAG_SELECT_CURSOR, 0, 0, 0, -1),
    Sql(Sql::do_rollback()),
    Sql(Sql::set_autocommit_on()),
    Sql("select count(*) from tt95a where c = 10",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    // multiple update
    Sql("-- multiple update and rollback"),
    Sql(Sql::set_autocommit_off()),
    Sql("update tt95a set c = 20 where c = 10",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    Sql("select count(*) from tt95a where c = 20",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql("update tt95a set c = 30 where c = 20",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    Sql("select count(*) from tt95a where c = 30",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql("update tt95a set c = 40 where c = 30",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    Sql("select count(*) from tt95a where c = 40",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql("update tt95a set c = 50 where c = 40",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    Sql("select count(*) from tt95a where c = 50",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    Sql(Sql::do_rollback()),
    Sql(Sql::set_autocommit_on()),
    Sql("select count(*) from tt95a where c = 10",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1, -1),
    // another variant which found no tuple via index (aligment issue)
    Sql("drop table tt95b",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table tt95b (a int primary key, b char(10) not null, c int not null)",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    Sql("create index xx95b on tt95b (b, c) nologging",
	SQL_DIAG_CREATE_INDEX, -1, -1, -1, -1),
    Sql("insert into tt95b values(0,'0123456789',1)",
	SQL_DIAG_INSERT, 1, -1, -1, -1),
    Sql("select a from tt95b where b='0123456789'",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 0, -1),
    // update index key to different value
    Sql("update tt95b set b = '9876543210' where b = '0123456789'",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
    // same value goes nuts...
    Sql("update tt95b set b = '9876543210'",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
#if 0
    // ...if done via index key (variant of halloween problem)
    Sql("update tt95b set b = '9876543210' where b = '9876543210'",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, -1),
#endif
    Sql()
};

// 96

static void
testMisc96a(Test& test)
{
    // single thread
    if (test.m_no != 1)
	return;
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    char sql[MAX_SQL], *sqlptr;
    char tname[20];
    strcpy(tname, "tt96a");
    // drop table
    scopy(sqlptr = sql, "drop table %s", tname);
    test.exp(SQL_ERROR, "IM000", 2040709, false);
    if (opt.m_v >= 2)
        ndbout << "SQL: " << sql << endl;
    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
    // create table with many attributes
    unsigned attrs = 1 + opt.m_scale;
    if (attrs > MAX_ATTRIBUTES_IN_TABLE)
        attrs = MAX_ATTRIBUTES_IN_TABLE;
    if (attrs > 64)
	attrs = 64;
    scopy(sqlptr = sql, "create table %s (c0 int primary key", tname);
    for (unsigned j = 1; j < attrs; j++) {
        if (j % 2 == 0)
            scopy(sqlptr, ", c%d int unsigned not null", j);
        else
            scopy(sqlptr, ", c%d char(10) not null", j);
    }
    scopy(sqlptr, ")");
    if (opt.m_fragtype != 0)
        scopy(sqlptr, " storage(%s)", opt.m_fragtype);
    if (opt.m_v >= 2)
        ndbout << "SQL: " << sql << endl;
    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
    // create or drop indexes
    const unsigned seed = 1000037 * test.m_loop + 1000039 * opt.m_scale;
    srandom(seed);
    const unsigned imax = opt.m_scale < 20 ? opt.m_scale : 20;
    AttributeMask* imasks = new AttributeMask[imax];
    unsigned ccnt = 0;
    unsigned dcnt = 0;
    for (unsigned n = 0; n < imax; n++)
	imasks[n].clear();
    while (ccnt + dcnt < opt.m_scale) {
	char iname[20];
	unsigned n = urandom(imax);
	sprintf(iname, "xx96a%02d", n);
	AttributeMask& imask = imasks[n];
	unsigned sel = urandom(10);
	if (imask.isclear()) {
	    // create one
	    unsigned ncol = 0;
	    unsigned cols[MAX_ATTRIBUTES_IN_INDEX];
	    unsigned cnum = urandom(attrs);
	    cols[ncol++] = cnum;
	    while (ncol < MAX_ATTRIBUTES_IN_INDEX) {
		unsigned sel2 = urandom(10);
		if (sel2 < 2)
		    break;
		unsigned cnum2 = urandom(attrs);
		if (sel2 < 9 && cnum2 == 0)
		    continue;
		unsigned j;
		for (j = 0; j < ncol; j++) {
		    if (cols[j] == cnum2)
			break;
		}
		if (j == ncol)
		    cols[ncol++] = cnum2;
	    }
	    if (sel < 3) {
		scopy(sqlptr = sql, "create unique hash index %s on %s (", iname, tname);
		for (unsigned j = 0; j < ncol; j++)
		    scopy(sqlptr, "%sc%d", j == 0 ? "" : ", ", cols[j]);
		scopy(sqlptr, ")");
	    } else {
		scopy(sqlptr = sql, "create index %s on %s (", iname, tname);
		for (unsigned j = 0; j < ncol; j++)
		    scopy(sqlptr, "%sc%d", j == 0 ? "" : ", ", cols[j]);
		scopy(sqlptr, ") nologging");
	    }
	    if (opt.m_v >= 2)
		ndbout << "SQL: " << sql << endl;
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    for (unsigned j = 0; j < ncol; j++)
		imask.set(cols[j]);
	    ccnt++;
	} else if (sel < 5 && ccnt > dcnt + 1) {
	    scopy(sqlptr = sql, "drop index %s on %s", iname, tname);
	    if (opt.m_v >= 2)
		ndbout << "SQL: " << sql << endl;
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    imask.clear();
	    dcnt++;
	}
    }
    // insert unique data
    unsigned rows = opt.m_scale;
    unsigned* uval = new unsigned[rows];
    for (unsigned i = 0; i < rows; i++) {
	uval[i] = urandom(4);
	scopy(sqlptr = sql, "insert into %s values(", tname);
	for (unsigned j = 0; j < attrs; j++) {
	    if (j != 0)
		scopy(sqlptr, ",");
	    unsigned v = (i << 10) | (j << 2) | uval[i];
            if (j == 0)
		scopy(sqlptr, "%u", i);
            else if (j % 2 == 0)
		scopy(sqlptr, "%u", v);
	    else
		scopy(sqlptr, "'%010u'", v);
	}
	scopy(sqlptr, ")");
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
    }
    // update each row via random index
    for (unsigned i = 0; i < rows; i++) {
	unsigned uold = uval[i];
	uval[i] = 3 - uval[i];
	AttributeMask imask;
	do {
	    unsigned j = urandom(imax);
	    imask = imasks[j];
	} while (imask.isclear());
	scopy(sqlptr = sql, "update %s set", tname);
	for (unsigned j = 1; j < attrs; j++) {
	    if (j != 1)
		scopy(sqlptr, ",");
	    /*
	     * Equality update is just barely doable before savepoints
	     * provided we change value of keys in every index.
	     */
	    unsigned v = (i << 10) | (j << 2) | uval[i];
            if (j == 0)
		;
	    else if (j % 2 == 0)
		scopy(sqlptr, " c%d=%u", j, v);
	    else
		scopy(sqlptr, " c%d='%010u'", j, v);
	}
	scopy(sqlptr, " where 1=1");
	while (! imask.isclear()) {
	    unsigned j = urandom(attrs);
	    if (imask.get(j)) {
		unsigned v = (i << 10) | (j << 2) | uold;
		scopy(sqlptr, " and c%d=", j);
		if (j == 0)
		    scopy(sqlptr, "%u", i);
		else if (j % 2 == 0)
		    scopy(sqlptr, "%u", v);
		else
		    scopy(sqlptr, "'%010u'", v);
		imask.clear(j);
	    }
	}
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << endl;
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	chkRowCount(test, hStmt, 1);
    }
    // delete all
    scopy(sqlptr = sql, "delete from %s", tname);
    if (opt.m_v >= 2)
        ndbout << "SQL: " << sql << endl;
    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
    //
    if (opt.m_v >= 2)
	ndbout << tname << ": creates " << ccnt << " drops " << dcnt << endl;
    delete [] imasks;
    delete [] uval;
    freeAll(test, hEnv, hDbc, hStmt);
}

static const Sql
miscSql96[] = {
    Sql("testMisc96a", testMisc96a),
    Sql()
};

// 97

static void
testMisc97a(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    const char* tname = "TT97A";
    const char* iname = "XX97A";
    char sql[MAX_SQL];
    // create in some thread
    lock_mutex();
    if (my_sema == 0) {
	if (opt.m_v >= 1)
	    ndbout << "thread " << test.m_no << " does setup" << endl;
	sprintf(sql, "drop table %s", tname);
	if (opt.m_v >= 2)
	    ndbout << "SQL[" << test.m_no << "]: " << sql << endl;
	test.exp(SQL_ERROR, "IM000", 2040709, false);
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	// a-pk b-index c-counter
	sprintf(sql, "create table %s (a int primary key, b int, c int) storage(small)", tname);
	if (opt.m_v >= 2)
	    ndbout << "SQL[" << test.m_no << "]: " << sql << endl;
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	for (unsigned i = 0; i < opt.m_scale; i++) {
	    sprintf(sql, "insert into %s values (%d, %d, %d)", tname, i, 10 * i, 0);
	    if (opt.m_v >= 3)
		ndbout << "SQL[" << test.m_no << "]: " << sql << endl;
	    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	}
	sprintf(sql, "create index %s on %s (b) nologging", iname, tname);
	if (opt.m_v >= 2)
	    ndbout << "SQL[" << test.m_no << "]: " << sql << endl;
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	my_sema = 1;
    }
    unlock_mutex();
    assert(my_sema == 1);
    // parallel run - default rotating pk, ts, is
    // frob: low 3 hex digits give alt sequence e.g. 0x311 = pk, pk, is
    // frob: 4-th hex digit non-zero says use NDB API e.g. 0x1000
    unsigned typelist[3] = { 1, 2, 3 };
    for (unsigned i = 0; i < 3; i++) {
	unsigned t = (opt.m_frob >> (i * 4)) & 0xf;
	if (t != 0)
	    typelist[i] = t;
    }
    unsigned type = typelist[(test.m_no - 1) % 3];
    if ((opt.m_frob & 0xf000) == 0) {
	for (unsigned i = 0; i < opt.m_scale; i++) {
	    if (type == 1) {
		// pk update
		sprintf(sql, "update %s set c = c + 1 where a = %d", tname, i % opt.m_scale);
		if (opt.m_v >= 3)
		    ndbout << lock << "SQL[" << test.m_no << "]: " << sql << endl << unlock;
		test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    }
	    if (type == 2) {
		// table scan update
		sprintf(sql, "update %s set c = c + 1 where b + 0 = %d", tname, 10 * i);
		if (opt.m_v >= 3)
		    ndbout << lock << "SQL[" << test.m_no << "]: " << sql << endl << unlock;
		test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    }
	    if (type == 3) {
		// index scan update
		sprintf(sql, "update %s set c = c + 1 where b = %d", tname, 10 * i);
		if (opt.m_v >= 3)
		    ndbout << lock << "SQL[" << test.m_no << "]: " << sql << endl << unlock;
		test.exp(SQL_NO_DATA, 0, 0, false);
		test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	    }
	}
    } else {
#ifdef ndbODBC
#define CHK(o, x) do { if (! (x)) { fatal("line %d: %d %s", __LINE__, o->getNdbError().code, o->getNdbError().message); } } while (0)
	Ndb* ndb = new Ndb("TEST_DB");
	ndb->init();
	CHK(ndb, ndb->waitUntilReady() == 0);
	Int32 a, b, c;
	for (unsigned i = 0; i < opt.m_scale; i++) {
	    if (type == 1) {
		// pk update with exclusive read
		NdbConnection* con;
		NdbOperation* op;
		CHK(ndb, (con = ndb->startTransaction()) != 0);
		a = i;
		c = -1;
		CHK(con, (op = con->getNdbOperation(tname)) != 0);
		CHK(op, op->readTupleExclusive() == 0);
		CHK(op, op->equal((unsigned)0, (char*)&a, 0) == 0);
		CHK(op, op->getValue(2, (char*)&c) != 0);
		CHK(con, con->execute(NoCommit) == 0);
		c = c + 1;
		CHK(con, (op = con->getNdbOperation(tname)) != 0);
		CHK(op, op->updateTuple() == 0);
		CHK(op, op->equal((unsigned)0, (char*)&a, 0) == 0);
		CHK(op, op->setValue(2, (char*)&c) == 0);
		CHK(con, con->execute(Commit) == 0);
		ndb->closeTransaction(con);
		if (opt.m_v >= 3)
		    ndbout << lock << "thr " << test.m_no << " pk a=" << i << " c=" << c << endl << unlock;
	    }
	    if (type == 2) {
		// table scan update
		NdbConnection* con;
		NdbOperation* op;
		CHK(ndb, (con = ndb->startTransaction()) != 0);
		CHK(con, (op = con->getNdbOperation(tname)) != 0);
		CHK(con, op->openScanExclusive(240) == 0);
		CHK(op, op->getValue((unsigned)0, (char*)&a) != 0);
		CHK(op, op->getValue(2, (char*)&c) != 0);
		CHK(con, con->executeScan() == 0);
		unsigned rows = 0;
		unsigned updates = 0;
		while (1) {
		    int ret;
		    a = -1;
		    c = -1;
		    CHK(con, (ret = con->nextScanResult()) == 0 || ret == 1);
		    if (ret == 1)
			break;
		    rows++;
		    if (a == i) {
			NdbConnection* con2;
			NdbOperation* op2;
			CHK(ndb, (con2 = ndb->startTransaction()) != 0);
			CHK(op, (op2 = op->takeOverForUpdate(con2)) != 0);
			c = c + 1;
			CHK(op2, op2->setValue(2, (char*)&c) == 0);
			CHK(con2, con2->execute(Commit) == 0);
			ndb->closeTransaction(con2);
			updates++;
			if (opt.m_v >= 3)
			    ndbout << lock << "thr " << test.m_no << " ts rows=" << rows << " a=" << i << " c=" << c << endl << unlock;
			// test stop scan too
			CHK(con, con->stopScan() == 0);
			break;
		    }
		}
		ndb->closeTransaction(con);
		test.chk(HStmt(hStmt), updates == 1, "got %u != 1", updates);
	    }
	    if (type == 3) {
		// index scan update
		NdbConnection* con;
		NdbOperation* op;
		CHK(ndb, (con = ndb->startTransaction()) != 0);
		CHK(con, (op = con->getNdbOperation(iname, tname)) != 0);
		CHK(con, op->openScanExclusive(240) == 0);
		b = 10 * i;
		CHK(con, op->setBound((unsigned)0, 4, &b, sizeof(b)) == 0);
		CHK(op, op->getValue((unsigned)0, (char*)&a) != 0);
		CHK(op, op->getValue(2, (char*)&c) != 0);
		CHK(con, con->executeScan() == 0);
		unsigned rows = 0;
		unsigned updates = 0;
		while (1) {
		    int ret;
		    a = -1;
		    c = -1;
		    CHK(con, (ret = con->nextScanResult()) == 0 || ret == 1);
		    if (ret == 1)
			break;
		    rows++;
		    if (a == i) {
			NdbConnection* con2;
			NdbOperation* op2;
			CHK(ndb, (con2 = ndb->startTransaction()) != 0);
			CHK(op, (op2 = op->takeOverForUpdate(con2)) != 0);
			c = c + 1;
			CHK(op2, op2->setValue(2, (char*)&c) == 0);
			CHK(con2, con2->execute(Commit) == 0);
			ndb->closeTransaction(con2);
			updates++;
			if (opt.m_v >= 3)
			    ndbout << lock << "thr " << test.m_no << " is rows=" << rows << " a=" << i << " c=" << c << endl << unlock;
			// test stop scan too
			CHK(con, con->stopScan() == 0);
			break;
		    }
		}
		ndb->closeTransaction(con);
		test.chk(HStmt(hStmt), rows == 1, "got %u != 1", rows);
		test.chk(HStmt(hStmt), updates == 1, "got %u != 1", updates);
	    }
	}
	delete ndb;
#undef CHK
#endif
    }
    // verify result
    lock_mutex();
    if (++my_sema == 1 + opt.m_threads) {
	if (opt.m_v >= 1)
	    ndbout << "thread " << test.m_no << " does verification" << endl;
	sprintf(sql, "select * from %s order by a", tname);
	if (opt.m_v >= 2)
	    ndbout << "SQL[" << test.m_no << "]: " << sql << endl;
	test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
	long a, b, c;
	test.run(HStmt(hStmt), SQLBindCol(hStmt, 1, SQL_C_SLONG, &a, 0, 0));
	test.run(HStmt(hStmt), SQLBindCol(hStmt, 2, SQL_C_SLONG, &b, 0, 0));
	test.run(HStmt(hStmt), SQLBindCol(hStmt, 3, SQL_C_SLONG, &c, 0, 0));
	for (unsigned i = 0; i < opt.m_scale; i++) {
	    a = b = c = -1;
	    test.run(HStmt(hStmt), SQLFetch(hStmt));
	    test.chk(HStmt(hStmt), a == i, "a: got %ld != %u", a, i);
	    test.chk(HStmt(hStmt), b == 10 * i, "b: got %ld != %u", b, 10 * i);
	    test.chk(HStmt(hStmt), c == opt.m_threads, "c: got %ld != %u", c, opt.m_threads);
	    if (opt.m_v >= 4)
		ndbout << "verified " << i << endl;
	}
	test.exp(SQL_NO_DATA, 0, 0, true);
	test.run(HStmt(hStmt), SQLFetch(hStmt));
	if (opt.m_v >= 2)
	    ndbout << "thr " << test.m_no << " verified " << opt.m_scale << " rows" << endl;
	my_sema = 0;
    }
    unlock_mutex();
    freeAll(test, hEnv, hDbc, hStmt);
}

static const Sql
miscSql97[] = {
    Sql("testMisc97a", testMisc97a),
    Sql()
};

// 99

static void
testMisc99a(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    // bad
    const char* sqlInsertBad = "insert into tt99a values(?, ?, ?, ?, ?)";
    test.exp(SQL_ERROR, "21S01", -1, true);
    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sqlInsertBad, SQL_NTS));
    // good
    const char* sqlInsert = "insert into tt99a (col1, col2, col3, col4, col5) values(?, ?, ?, ?, ?)";
    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sqlInsert, SQL_NTS));
    unsigned long value;
    for (unsigned i = 1; i <= 5; i++) {
	test.run(HStmt(hStmt), SQLBindParameter(hStmt, i, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &value, 0, 0));
    }
    const unsigned long base = 1000000000;
    const unsigned long scale = 10;
    for (value = base; value < base + scale; value++) {
	test.run(HStmt(hStmt), SQLExecute(hStmt));
    }
    // bug1: re-analyze of converted expression...
    const char* sqlSelect = "select col5 from tt99a where col2 + 0 = ?";
    unsigned long output;
    test.run(HStmt(hStmt), SQLBindCol(hStmt, 1, SQL_C_ULONG, &output, 0, 0));
    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sqlSelect, SQL_NTS));
    // bug2: previous bind must survive a new SQLPrepare
    if (0) {
	test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &value, 0, 0));
    }
    for (value = base; value < base + scale; value++) {
	if (value > base + 4) {
	    // bug1: ...when IPD changed by JDBC
	    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &value, 0, 0));
	}
	test.run(HStmt(hStmt), SQLExecute(hStmt));
	output = (unsigned long)-1;
	test.run(HStmt(hStmt), SQLFetch(hStmt));
	test.chk(HStmt(hStmt), output == value, "got %lu != %lu", output, value);
	test.exp(SQL_NO_DATA, 0, 0, true);
	test.run(HStmt(hStmt), SQLFetch(hStmt));
	test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
	test.timerCnt(1);
    }
    freeAll(test, hEnv, hDbc, hStmt);
}

static void
testMisc99c(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    const char* sql = "select b from tt99c where a = ?";
    const unsigned long c1 = 2100000000U;
    const unsigned long c2 = 4100000000U;
    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
    unsigned long aval, bval;
    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &aval, 0, 0));
    test.run(HStmt(hStmt), SQLBindCol(hStmt, 1, SQL_C_ULONG, &bval, 0, 0));
    // uno
    for (unsigned i = 0; i < opt.m_scale; i++) {
	aval = c1;
	bval = (unsigned long)-1;
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << " [?=" << (Uint64)aval << "]" << endl;
	test.run(HStmt(hStmt), SQLExecute(hStmt));
	test.run(HStmt(hStmt), SQLFetch(hStmt));
	test.chk(HStmt(hStmt), bval == c2, "got %lu != %lu", bval, c2);
	//test.exp(SQL_NO_DATA, 0, 0, true);
	//test.run(HStmt(hStmt), SQLFetch(hStmt));
	test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
    }
    // dos
    for (unsigned i = 0; i < opt.m_scale; i++) {
	break;	// XXX not yet, hangs in NDB ?!?
	aval = c2;
	bval = (unsigned long)-1;
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql << " [?=" << (Uint64)aval << "]" << endl;
	test.run(HStmt(hStmt), SQLExecute(hStmt));
	test.run(HStmt(hStmt), SQLFetch(hStmt));
	test.chk(HStmt(hStmt), bval == c1, "got %lu != %lu", bval, c2);
	//test.exp(SQL_NO_DATA, 0, 0, true);
	//test.run(HStmt(hStmt), SQLFetch(hStmt));
	test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
    }
    freeAll(test, hEnv, hDbc, hStmt);
}

static void
testMisc99d(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    const char* tname = "TT99D";
    char sql[MAX_SQL];
    sprintf(sql, "drop table %s", tname);
    if (opt.m_v >= 2)
	ndbout << "SQL: " << sql << endl;
    test.exp(SQL_ERROR, "IM000", 2040709, false);
    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
    sprintf(sql, "create table %s (a bigint unsigned, b bigint, primary key (a))", tname);
    if (opt.m_v >= 2)
	ndbout << "SQL: " << sql << endl;
    test.run(HStmt(hStmt), SQLExecDirect(hStmt, (SQLCHAR*)sql, SQL_NTS));
    sprintf(sql, "insert into %s values (?, ?)", tname);
    if (opt.m_v >= 2)
	ndbout << "SQL: " << sql << endl;
    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
    // XXX replace by 100 when signed vs unsigned resolved
    const unsigned num = 78;
    SQLUBIGINT aval;
    SQLBIGINT bval;
    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_UBIGINT, SQL_BIGINT, 0, 0, &aval, 0, 0));
    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0, &bval, 0, 0));
    for (SQLBIGINT i = 0; i < num; i++) {
	if (opt.m_v >= 3)
	    ndbout << "insert " << i << endl;
	aval = i * i * i * i * i * i * i * i * i * i; 	// 10
	bval = -aval;
	test.run(HStmt(hStmt), SQLExecute(hStmt));
    }
    sprintf(sql, "select a, b from tt99d where a = ?");
    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql, SQL_NTS));
    SQLUBIGINT kval;
    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_UBIGINT, SQL_BIGINT, 0, 0, &kval, 0, 0));
    test.run(HStmt(hStmt), SQLBindCol(hStmt, 1, SQL_C_UBIGINT, &aval, 0, 0));
    test.run(HStmt(hStmt), SQLBindCol(hStmt, 2, SQL_C_SBIGINT, &bval, 0, 0));
    for (SQLBIGINT i = 0; i < num; i++) {
	kval = i * i * i * i * i * i * i * i * i * i;	// 10
	if (opt.m_v >= 3)
	    ndbout << "fetch " << i << " key " << kval << endl;
	test.run(HStmt(hStmt), SQLExecute(hStmt));
	aval = bval = 0;
	test.run(HStmt(hStmt), SQLFetch(hStmt));
	test.chk(HStmt(hStmt), aval == kval && bval == -kval, "got %llu, %lld != %llu, %lld", aval, bval, kval, -kval);
	test.exp(SQL_NO_DATA, 0, 0, true);
	test.run(HStmt(hStmt), SQLFetch(hStmt));
	test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
    }
    freeAll(test, hEnv, hDbc, hStmt);
}

static void
testMiscC2(Test& test)
{
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
#if 0
 {
    char POP[255];
    char PORT[255];
    char ACCESSNODE[255];

    const char* sqlSelect = "select PORT from AAA where POP=? and ACCESSNODE=?";

    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sqlSelect, SQL_NTS));

    for (int j=0; j<5; j++) {
      printf("Loop %u\n", j);
      printf("LINE %u\n", __LINE__);

      test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 255, 0, POP, 255, 0));
      test.run(HStmt(hStmt), SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 255, 0, ACCESSNODE, 255, 0));
      test.run(HStmt(hStmt), SQLBindCol(hStmt, 1, SQL_C_CHAR, PORT, 255, 0));

      sprintf(POP, "a");
      sprintf(ACCESSNODE, "b");

      test.run(HStmt(hStmt), SQLExecute(hStmt));
      test.run(HStmt(hStmt), SQLFetch(hStmt));
      printf("got %s\n", PORT);
      printf("LINE %u\n", __LINE__);
      test.exp(SQL_NO_DATA, 0, 0, true);
      printf("LINE %u\n", __LINE__);
      test.run(HStmt(hStmt), SQLFetch(hStmt));
      printf("LINE %u\n", __LINE__);
      test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
      printf("LINE %u\n", __LINE__);
    }
 }
    return;
#endif

    char POP[255];
    char PORT[255];
    char ACCESSNODE[255];
    unsigned long VLAN = 0;
    unsigned long SNMP_INDEX = 0;
    unsigned long PORT_STATE = 0;
    unsigned long STATIC_PORT = 0;
    unsigned long COMMENT = 0;

    const char* sqlSelect = "select PORT, PORT_STATE  from PORTS where POP=? and ACCESSNODE=?";

    test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sqlSelect, SQL_NTS));

    for (int j=0; j<5; j++) {
      printf("Loop %u\n", j);
      printf("LINE %u\n", __LINE__);

    test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 255, 0, POP, 255, 0));
      test.run(HStmt(hStmt), SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 255, 0, ACCESSNODE, 255, 0));
      test.run(HStmt(hStmt), SQLBindCol(hStmt, 1, SQL_C_CHAR, PORT, 255, 0));
      test.run(HStmt(hStmt), SQLBindCol(hStmt, 2, SQL_C_ULONG, &PORT_STATE, 0, 0));

      sprintf(POP, "row%u.i%u.bredband.com", 2, 3);
      sprintf(ACCESSNODE, "as%u", 2);

      test.run(HStmt(hStmt), SQLExecute(hStmt));
      for (int i=0; i < 3; i++) {
	PORT_STATE=0;
	sprintf(PORT, "XXXXXXXXXXXXXXXXXXXXX");
	
	test.run(HStmt(hStmt), SQLFetch(hStmt));
	printf("got %s %lu\n", PORT, PORT_STATE);
	//    test.chk(HStmt(hStmt), false, "got %s != %s", "xxx", PORT);
      }
      printf("LINE %u\n", __LINE__);
      test.exp(SQL_NO_DATA, 0, 0, true);
      printf("LINE %u\n", __LINE__);
      test.run(HStmt(hStmt), SQLFetch(hStmt));
      printf("LINE %u\n", __LINE__);
      test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
      printf("LINE %u\n", __LINE__);
    }
}

static const Sql
miscSqlC2[] = {
    Sql("drop table PORTS",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table PORTS (POP varchar(200) not null, ACCESSNODE varchar(200) not null, PORT varchar(200) not null, VLAN int unsigned, SNMP_INDEX int unsigned, PORT_STATE int unsigned, STATIC_PORT int unsigned, COMMENT int unsigned, primary key (POP,ACCESSNODE,PORT))",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    Sql("create index xxPORTS on PORTS (POP, ACCESSNODE) nologging",
    	SQL_DIAG_CREATE_INDEX, -1, -1, -1, -1),
    Sql("insert into PORTS values ('row2.i3.bredband.com','as2','Fa0/0',0,1,2,3,4)",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    Sql("insert into PORTS values ('row2.i3.bredband.com','as2','Fa0/1',1,2,3,4,5)",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    Sql("insert into PORTS values ('row2.i3.bredband.com','as2','Fa0/2',2,3,4,5,6)",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    Sql("select PORT, PORT_STATE  from PORTS where POP='row2.i3.bredband.com' and ACCESSNODE='as2'",
	SQL_DIAG_SELECT_CURSOR, 3, 3, -1, -1),

    Sql("drop table AAA",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table AAA (POP varchar(200), ACCESSNODE varchar(200) not null, PORT varchar(200) not null, primary key (POP,ACCESSNODE,PORT))",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    Sql("create index xxAAA on AAA (POP, ACCESSNODE) nologging",
    	SQL_DIAG_CREATE_INDEX, -1, -1, -1, -1),
    Sql("insert into AAA values ('a','b','A')",
	SQL_DIAG_INSERT, 1, 0, -1, -1),

    Sql("testMiscC2", testMiscC2),
    Sql()
};

/*
> SELECT PORT, PORT_STATE  FROM PORTS where pop=? and accessnode=?
> SELECT VLAN, SNMP_INDEX, PORT_STATE, STATIC_PORT, COMMENT FROM PORTS WHERE POP=? AND ACCESSNODE=? AND PORT=?
> select count(*) from ports
> select snmp_index from ports where pop='row2.i3.bredband.com' and accessnode='as2' and port='Fa0/2'

> SELECT MAC, MAC_EXPIRE, IP, IP_EXPIRE, HOSTNAME, DETECTED, STATUS, STATIC_DNS, BLOCKED, NUM_REQUESTS, ACCESSTYPE, OS_TYPE, GATE_WAY, DIRTY_FLAG, LOCKED_IP FROM CLIENTS WHERE PORT=? AND ACCESSNODE=? AND POP=?
> SELECT SERVICES.ACCESSTYPE, SERVICES.NUM_IP, SERVICES.TEXPIRE, SERVICES.CUSTOMER_ID, SERVICES.LEASED_NUM_IP, SERVICES.PROVIDER, SERVICES.LOCKED_IP, SERVICES.STATIC_DNS, SERVICES.SUSPENDED_SERVICE FROM SERVICES , ACCESSTYPES WHERE SERVICES.PORT =  ? AND SERVICES.ACCESSNODE = ? AND  SERVICES.POP =  ? AND SERVICES.ACCESSTYPE=ACCESSTYPES.ACCESSTYPE
*/

static const Sql
miscSql99[] = {
    Sql("drop table tt99a",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table tt99a (col1 int unsigned primary key, col2 int unsigned, col3 int unsigned, col4 int unsigned, col5 int unsigned, col6 varchar(7) default 'abc123')",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    // inserts 10 rows, all same, start value 1000000000
    Sql("testMisc99a", testMisc99a),
    // interpreted scan plus bind parameter
    Sql("select col1 from tt99a where col2 = ?",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1000000004, 1000000004),
    Sql("select col1 from tt99a where col2 = 1000000000 + ?",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1000000004, 4),
    Sql("select col1 from tt99a where col2 = ? + 1000000000",
	SQL_DIAG_SELECT_CURSOR, 1, 1, 1000000004, 4),
    // same not interpreted, tuple count 10
    Sql("select col1 from tt99a where col2 + 0 = 1000000000 + ?",
	SQL_DIAG_SELECT_CURSOR, 1, 10, 1000000004, 4),
    // varchar variations
    Sql("select count(*) from tt99a where col6 = 'abc123'",
	SQL_DIAG_SELECT_CURSOR, 1, 10, 10, -1),
    Sql("select count(*) from tt99a where left(col6, ?) = 'abc1'",
	SQL_DIAG_SELECT_CURSOR, 1, 10, 10, 4),
    Sql("select count(*) from tt99a where left(col6, ?) = 'abc1'",
	SQL_DIAG_SELECT_CURSOR, 1, 10, 0, 3),
    // tpc-b inspired, wrong optimization to direct update
    Sql("drop table tt99b",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table tt99b(a int primary key, b int not null, c double precision)",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    Sql("insert into tt99b values(1, 10, 100.0)",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    Sql("insert into tt99b values(9, 90, 900.0)",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    Sql("create unique hash index tt99y on tt99b (b)",
	SQL_DIAG_CREATE_INDEX, -1, 0, -1, -1),
    // first scan update..
    Sql("update tt99b set c = c + ? where a+0 = 1",
	SQL_DIAG_UPDATE_WHERE, 1, 2, -1, 10),
    Sql("update tt99b set c = c + ? where b+0 = 10",
	SQL_DIAG_UPDATE_WHERE, 1, 2, -1, 10),
    // then optimized..
    Sql("update tt99b set c = c + ? where a = 1",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, 10),
    Sql("update tt99b set c = c + ? where b = 10",
	SQL_DIAG_UPDATE_WHERE, 1, 1, -1, 10),
    // verify..
    Sql("select count(*) from tt99b where 100-1 < c and c < 140-1",
	SQL_DIAG_SELECT_CURSOR, 1, 2, 0, -1),
    Sql("select count(*) from tt99b where 140-.001 < c and c < 140+.001",
	SQL_DIAG_SELECT_CURSOR, 1, 2, 1, -1),
    // unsigned test
    Sql("drop table tt99c",
	SQL_DIAG_DROP_TABLE, -1, 0, -1, -1,
	SQL_ERROR, "IM000", 2040709, false),
    Sql("create table tt99c(a int unsigned primary key, b int unsigned)",
	SQL_DIAG_CREATE_TABLE, -1, 0, -1, -1),
    Sql("insert into tt99c values(2100000000, 4100000000)",
	SQL_DIAG_INSERT, 1, 0, -1, -1),
    Sql("insert into tt99c (a, b) select b, a from tt99c",
	SQL_DIAG_INSERT, 1, 1, -1, -1),
    Sql("testMisc99c", testMisc99c),
    // new external type SQL_C_[SU]BIGINT
    Sql("testMisc99d", testMisc99d),
    Sql()
};

static const struct { const Sql* sql; int minscale; }
miscSql[11] = {
    {	miscSql90,	0	},
    {	miscSql91,	0	},
    {	miscSql92,	0	},
    {	miscSql93,	0	},
    {	0,		0	},	// 94
    {	miscSql95,	0	},
    {	miscSql96,	0	},
    {	miscSql97,	0	},
    {	0,		0	},	// 98
    {	miscSql99,	0	},
    {	miscSqlC2, 	0	}
};

static void
testSql(Test& test)
{
    const unsigned salt = test.m_stuff;		// mess
    if (opt.m_scale < miscSql[salt].minscale) {
	if (opt.m_v >= 1)
	    ndbout << "skip - requires scale >= " << miscSql[salt].minscale << endl;
	return;
    }
    assert(0 <= salt && salt < 11 && miscSql[salt].sql != 0);
    SQLHANDLE hEnv, hDbc, hStmt;
    allocAll(test, hEnv, hDbc, hStmt);
    for (unsigned i = 0; ; i++) {
	const Sql& sql = miscSql[salt].sql[i];
	if (sql.m_sql == 0)
	    break;
	if (opt.m_v >= 2)
	    ndbout << "SQL: " << sql.m_sql << endl;
	if (sql.m_testFunc != 0) {
	    (*sql.m_testFunc)(test);
	    continue;
	}
	if (strncmp(sql.m_sql, "--", 2) == 0) {
	    continue;
	}
	if (strcmp(sql.m_sql, Sql::set_autocommit_on()) == 0) {
	    setAutocommit(test, hDbc, true);
	    continue;
	}
	if (strcmp(sql.m_sql, Sql::set_autocommit_off()) == 0) {
	    setAutocommit(test, hDbc, false);
	    continue;
	}
	if (strcmp(sql.m_sql, Sql::do_commit()) == 0) {
	    test.run(HDbc(hDbc), SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_COMMIT));
	    continue;
	}
	if (strcmp(sql.m_sql, Sql::do_rollback()) == 0) {
	    test.run(HDbc(hDbc), SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_ROLLBACK));
	    continue;
	}
	if (opt.m_v >= 3) {
	    ndbout <<  "expect:";
	    ndbout << " ret=" << sql.m_ret;
	    ndbout << " rows=" << sql.m_rowCount;
	    ndbout << " tuples=" << sql.m_tuplesFetched;
	    ndbout << endl;
	}
	test.run(HStmt(hStmt), SQLFreeStmt(hStmt, SQL_UNBIND));
	test.run(HStmt(hStmt), SQLFreeStmt(hStmt, SQL_RESET_PARAMS));
	// prep
	test.exp(sql.m_ret, sql.m_state, sql.m_native, false);
	test.run(HStmt(hStmt), SQLPrepare(hStmt, (SQLCHAR*)sql.m_sql, SQL_NTS));
	if (test.m_ret != SQL_SUCCESS)
	    continue;
	// bind between prep and exec like JDBC
	unsigned long bindValue = sql.m_bindValue;
	for (int k = 0; k <= 1; k++) {
	    if (bindValue != -1) {
		assert(strchr(sql.m_sql, '?') != 0);
		test.run(HStmt(hStmt), SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0, &bindValue, 0, 0));
	    }
	    if (k == 0) {
		if (bindValue != -1) {
		    test.run(HStmt(hStmt), SQLFreeStmt(hStmt, SQL_RESET_PARAMS));
		    // exec with unbound parameter
		    test.exp(SQL_ERROR, "HY010", -1, true);
		    test.run(HStmt(hStmt), SQLExecute(hStmt));
		    test.chk(HStmt(hStmt), test.m_functionCode == sql.m_functionCode || sql.m_functionCode == -1, "func: got %d != %d", (int)test.m_functionCode, (int)sql.m_functionCode);
		}
	    } else {
		// exec
		test.exp(sql.m_ret, sql.m_state, sql.m_native, sql.m_reset);
		test.run(HStmt(hStmt), SQLExecute(hStmt));
		test.chk(HStmt(hStmt), test.m_functionCode == sql.m_functionCode || sql.m_functionCode == -1, "func: got %d != %d", (int)test.m_functionCode, (int)sql.m_functionCode);
	    }
	}
	if (sql.m_rowCount != -1) {
	    if (sql.m_functionCode == SQL_DIAG_SELECT_CURSOR) {
		long lastValue;
		if (sql.m_lastValue != -1)
		    test.run(HStmt(hStmt), SQLBindCol(hStmt, 1, SQL_C_SLONG, &lastValue, 0, 0));
		unsigned k = 0;
		do {
		    int rowCount = 0;
		    lastValue = -1;
		    while (1) {
			test.exp(SQL_NO_DATA, 0, 0, false);
			test.run(HStmt(hStmt), SQLFetch(hStmt));
			if (test.m_ret == SQL_NO_DATA)
			    break;
			rowCount++;
		    }
		    test.chk(HStmt(hStmt), rowCount == sql.m_rowCount, "rowCount: got %d != %d", (int)rowCount, (int)sql.m_rowCount);
		    if (sql.m_tuplesFetched != -1)
			chkTuplesFetched(test, hStmt, sql.m_tuplesFetched);
		    if (rowCount > 0 && sql.m_lastValue != -1)
			test.chk(HStmt(hStmt), lastValue == sql.m_lastValue, "lastValue: got %ld != %ld", (long)lastValue, (long)sql.m_lastValue);
		    test.run(HStmt(hStmt), SQLCloseCursor(hStmt));
		    if (++k >= opt.m_scale)
			break;
		    test.run(HStmt(hStmt), SQLExecute(hStmt));
		} while (1);
		test.timerCnt(opt.m_scale);
	    } else {
		assert(sql.m_lastValue == -1);
		chkRowCount(test, hStmt, sql.m_rowCount);
		if (sql.m_tuplesFetched != -1)
		    chkTuplesFetched(test, hStmt, sql.m_tuplesFetched);
		test.timerCnt(1);
	    }
	}
    }
    freeAll(test, hEnv, hDbc, hStmt);
}

// name, function, runmode, salt (0=const or n/a), description
static const Case caseList[] = {
    Case( "00alloc",	testAlloc,	Case::Thread,	0,	"allocate handles"		),
    Case( "01create",	testCreate,	Case::Single,	0,	"create tables for the test"	),
    Case( "02prepare",	testPrepare,	Case::Thread,	0,	"prepare without execute"	),
    Case( "03catalog",	testCatalog,	Case::Thread,	0,	"catalog functions"		),
    Case( "10insert",	testInsert,	Case::Thread,	1,	"insert computed rows"		),
    Case( "11delall",	testDeleteAll,	Case::Single,	0,	"delete all rows via scan"	),
    Case( "12insert",	testInsert,	Case::Thread,	1,	"insert computed rows again"	),
    Case( "13count",	testCount,	Case::Single,	0,	"count rows"			),
    Case( "14verpk",	testVerifyPk,	Case::Thread,	1,	"verify via primary key"	),
    Case( "15verscan",	testVerifyScan,	Case::Serial,	1,	"verify via range scans"	),
    Case( "16join",	testJoin,	Case::Single,	0,	"multiple self-join"		),
    Case( "17cart",	testCart,	Case::Single,	0,	"cartesian join"		),
    Case( "20updpk",	testUpdatePk,	Case::Thread,	2,	"update via primary key"	),
    Case( "21verpk",	testVerifyPk,	Case::Thread,	2,	"verify via primary key"	),
    Case( "22verscan",	testVerifyScan,	Case::Serial,	2,	"verify via range scans"	),
    Case( "23updscan",	testUpdateScan,	Case::Serial,	0,	"update via scan"		),
    Case( "24verpk",	testVerifyPk,	Case::Thread,	0,	"verify via primary key"	),
    Case( "25verscan",	testVerifyScan,	Case::Serial,	0,	"verify via range scans"	),
    Case( "26delpk",	testDeletePk,	Case::Thread,	0,	"delete via primary key"	),
    Case( "30trans",	testTrans,	Case::Single,	3,	"rollback and commit"		),
    Case( "31concur",	testConcur,	Case::Single,	0,	"commit across open cursor"	),
    Case( "32readcom",	testReadcom,	Case::Single,	0,	"read committed"		),
    Case( "40perf",	testPerf,	Case::Single,	0,	"perf test prepare"		),
    Case( "41perf",	testPerf,	Case::Thread,	1,	"perf test NDB API"		),
    Case( "42perf",	testPerf,	Case::Thread,	2,	"perf test NDB ODBC"		),
    Case( "90sql",	testSql,	Case::Single,	0,	"misc SQL: metadata"		),
    Case( "91sql",	testSql,	Case::Single,	1,	"misc SQL: misc"		),
    Case( "92sql",	testSql,	Case::Thread,	2,	"misc SQL: scan rollback"	),
    Case( "93sql",	testSql,	Case::Single,	3,	"misc SQL: locking"		),
    Case( "95sql",	testSql,	Case::Single,	5,	"misc SQL: indexes (simple)"	),
    Case( "96sql",	testSql,	Case::Single,	6,	"misc SQL: indexes"		),
    Case( "97sql",	testSql,	Case::Thread,	7,	"misc SQL: indexes"		),
    Case( "99sql",	testSql,	Case::Single,	9,	"misc SQL: bug du jour"		),
    Case( "C2", 	testSql,	Case::Single,	10,	"misc SQL: C2"	                )
};

static const unsigned caseCount = arraySize(caseList);

static bool
findCase(const char* name)
{
    for (unsigned i = 0; i < caseCount; i++) {
	const Case& cc = caseList[i];
	if (strstr(cc.m_name, name) != 0)
	    return true;
    }
    return false;
}

static void
listCases()
{
    ndbout << "cases:" << endl;
    unsigned m = 0;
    for (unsigned i = 0; i < caseCount; i++) {
	const Case& cc = caseList[i];
	if (m < strlen(cc.m_name))
	    m = strlen(cc.m_name);
    }
    for (unsigned i = 0; i < caseCount; i++) {
	const Case& cc = caseList[i];
	char buf[200];
	sprintf(buf, "%-*s  [%-6s]  %s", m, cc.m_name, cc.modename(), cc.m_desc);
	ndbout << buf << endl;
    }
}

// threads

extern "C" { static void* testThr(void* arg); }

struct Thr {
    enum State {
	Wait = 1,		// wait for test case
	Run = 2,		// run the test case
	Done = 3,		// done with the case
	Exit = 4		// exit thread
    };
    unsigned m_no;		// thread number 1 .. max
    NdbThread* m_thr;		// thread id etc
    const Case* m_case;		// current case
    State m_state;		// condition variable
    NdbMutex* m_mutex;		// condition guard
    NdbCondition* m_cond;
    void* m_status;		// exit status (not used)
    Test m_test;		// test runner
    Thr(unsigned no, unsigned loop) :
	m_no(no),
	m_thr(0),
	m_case(0),
	m_state(Wait),
	m_mutex(NdbMutex_Create()),
	m_cond(NdbCondition_Create()),
	m_status(0),
	m_test(no, loop) {
    }
    ~Thr() {
	destroy();
	NdbCondition_Destroy(m_cond);
	NdbMutex_Destroy(m_mutex);
    }
    void create() {
	assert(m_thr == 0);
	m_thr = NdbThread_Create(testThr, (void**)this, 64*1024, "test", NDB_THREAD_PRIO_LOW);
    }
    void destroy() {
	if (m_thr != 0)
	    NdbThread_Destroy(&m_thr);
	m_thr = 0;
    }
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
	NdbThread_WaitFor(m_thr, &m_status);
	m_thr = 0;
    }
    // called from main
    void mainStart(const Case& cc) {
	lock();
	m_case = &cc;
	m_state = Run;
	signal();
	unlock();
    }
    void mainStop() {
	lock();
	while (m_state != Done) {
	    if (opt.m_v >= 4)
		ndbout << ::lock << "thr " << m_no << " [main] wait state=" << m_state << endl << ::unlock;
	    wait();
	}
	if (opt.m_v >= 4)
	    ndbout << ::lock << "thr " << m_no << " [main] done" << endl << ::unlock;
	m_state = Wait;
	unlock();
    }
    // run in thread
    void testSelf() {
	while (1) {
	    lock();
	    while (m_state != Run && m_state != Exit) {
		if (opt.m_v >= 4)
		    ndbout << ::lock << "thr " << m_no << " [self] wait state=" << m_state << endl << ::unlock;
		wait();
	    }
	    if (m_state == Run) {
		if (opt.m_v >= 4)
		    ndbout << ::lock << "thr " << m_no << " [self] run" << endl << ::unlock;
		assert(m_case != 0);
		m_test.timerOn();
		m_test.runCase(*m_case);
		m_test.timerOff();
		m_state = Done;
		if (opt.m_v >= 4)
		    ndbout << ::lock << "thr " << m_no << " [self] done" << endl << ::unlock;
		signal();
		unlock();
	    } else if (m_state == Exit) {
		unlock();
		break;
	    } else {
		assert(false);
	    }
	}
	if (opt.m_v >= 4)
	    ndbout << ::lock << "thr " << m_no << " [self] exit" << endl << ::unlock;
    }
};

static void*
testThr(void* arg)
{
    Thr& thr = *(Thr*)arg;
    thr.testSelf();
    return 0;
}

#ifdef DMALLOC
extern "C" {

static int malloc_bytes = 0;
static int free_bytes = 0;

static void
malloc_track(const char *file, const unsigned int line, const int func_id, const DMALLOC_SIZE byte_size, const DMALLOC_SIZE alignment, const DMALLOC_PNT old_addr, const DMALLOC_PNT new_addr)
{
    if (func_id == DMALLOC_FUNC_MALLOC) {
	malloc_bytes += byte_size;
	return;
    }
    if (func_id == DMALLOC_FUNC_FREE) {
	DMALLOC_SIZE size = 0;
	dmalloc_examine(old_addr, &size, 0, 0, 0);
	free_bytes += size;
	// XXX useless - byte_size and size are 0
	return;
    }
}

}
#endif	/* DMALLOC */

static void
testMain()
{
#ifndef NDB_LINUX	/* valgrind-1.0.3 does not support */
    NdbThread_SetConcurrencyLevel(opt.m_threads + 2);
#endif
#ifdef DMALLOC
    dmalloc_track(malloc_track);
#endif
    Test test(0, 0);
#ifdef ndbODBC
    Ndb* ndb = 0;
    if (1) {		// pre-alloc one Ndb object
	ndb = new Ndb("TEST_DB");
	ndb->init();
	if (ndb->waitUntilReady() != 0) {
	    ndbout << ndb->getNdbError() << endl;
	    fatal("waitUntilReady");
	}
    }
#endif
    for (unsigned loop = 1; opt.m_loop == 0 || loop <= opt.m_loop; loop++) {
	if (opt.m_v >= 2)
	    ndbout << "loop " << loop << endl;
	// create new set of threads in each loop
	Thr** thrList = new Thr* [1 + opt.m_threads];
	for (unsigned n = 1; n <= opt.m_threads; n++) {
	    Thr& thr = *(thrList[n] = new Thr(n, loop));
	    thr.create();
	    if (opt.m_v >= 4)
		ndbout << "thr " << n << " [main] created" << endl;
	}
#ifdef DMALLOC
	malloc_bytes = free_bytes = 0;
#endif
	for (unsigned i = 0; i < caseCount; i++) {
	    const Case& cc = caseList[i];
	    if (! cc.matchcase())
		continue;
	    if (opt.m_v >= 2)
		ndbout << "RUN: " << cc.m_name << " - " << cc.m_desc << endl;
	    test.timerOn();
	    for (unsigned subloop = 1; subloop <= opt.m_subloop; subloop++) {
		my_sema = 0;
		if (opt.m_v >= 3)
		    ndbout << "subloop " << subloop << endl;
		if (cc.m_mode == Case::Single) {
		    Thr& thr = *thrList[1];
		    thr.mainStart(cc);
		    thr.mainStop();
		    test.timerCnt(thr.m_test);
		} else if (cc.m_mode == Case::Serial) {
		    for (unsigned n = 1; n <= opt.m_threads; n++) {
			Thr& thr = *thrList[n];
			thr.mainStart(cc);
			thr.mainStop();
			test.timerCnt(thr.m_test);
		    }
		} else if (cc.m_mode == Case::Thread) {
		    for (unsigned n = 1; n <= opt.m_threads; n++) {
			Thr& thr = *thrList[n];
			thr.mainStart(cc);
		    }
		    for (unsigned n = 1; n <= opt.m_threads; n++) {
			Thr& thr = *thrList[n];
			thr.mainStop();
			test.timerCnt(thr.m_test);
		    }
		} else {
		    assert(false);
		}
	    }
	    test.timerOff();
	    if (opt.m_v >= 1)
		ndbout << cc.m_name << " total " << test << endl;
	}
#ifdef DMALLOC
	if (opt.m_v >= 9)	// XXX useless now
	    ndbout << "malloc " << malloc_bytes << " free " << free_bytes << " lost " << malloc_bytes - free_bytes << endl;
#endif
	// tell threads to exit
	for (unsigned n = 1; n <= opt.m_threads; n++) {
	    Thr& thr = *thrList[n];
	    thr.lock();
	    thr.m_state = Thr::Exit;
	    thr.signal();
	    thr.unlock();
	    if (opt.m_v >= 4)
		ndbout << "thr " << n << " [main] told to exit" << endl;
	}
	for (unsigned n = 1; n <= opt.m_threads; n++) {
	    Thr& thr = *thrList[n];
	    thr.join();
	    if (opt.m_v >= 4)
		ndbout << "thr " << n << " [main] joined" << endl;
	    delete &thr;
	}
	delete[] thrList;
    }
#ifdef ndbODBC
    delete ndb;
#endif
}

static bool
str2num(const char* arg, const char* str, unsigned* num, unsigned lo = 0, unsigned hi = 0)
{
    char* end = 0;
    long n = strtol(str, &end, 0);
    if (end == 0 || *end != 0 || n < 0) {
	ndbout << arg << " " << str << " is invalid number" << endl;
	return false;
    }
    if (lo != 0 && n < lo) {
	ndbout << arg << " " << str << " is too small min = " << lo << endl;
	return false;
    }
    if (hi != 0 && n > hi) {
	ndbout << arg << " " << str << " is too large max = " << hi << endl;
	return false;
    }
    *num = n;
    return true;
}

NDB_COMMAND(testOdbcDriver, "testOdbcDriver", "testOdbcDriver", "testOdbcDriver", 65535)
{
    while (++argv, --argc > 0) {
	const char* arg = argv[0];
	if (strcmp(arg, "-case") == 0) {
	    if (++argv, --argc > 0) {
		assert(opt.m_namecnt < arraySize(opt.m_name));
		opt.m_name[opt.m_namecnt++] = argv[0];
		if (findCase(argv[0]))
		    continue;
	    }
	}
	if (strcmp(arg, "-core") == 0) {
	    opt.m_core = true;
	    continue;
	}
	if (strcmp(arg, "-depth") == 0) {
	    if (++argv, --argc > 0) {
		if (str2num(arg, argv[0], &opt.m_depth))
		    continue;
	    }
	}
	if (strcmp(arg, "-dsn") == 0) {
	    if (++argv, --argc > 0) {
		opt.m_dsn = argv[0];
		continue;
	    }
	}
	if (strcmp(arg, "-frob") == 0) {
	    if (++argv, --argc > 0) {
		if (str2num(arg, argv[0], &opt.m_frob))
		    continue;
	    }
	}
	if (strcmp(arg, "-errs") == 0) {
	    if (++argv, --argc > 0) {
		if (str2num(arg, argv[0], &opt.m_errs))
		    continue;
	    }
	}
	if (strcmp(arg, "-fragtype") == 0) {
	    if (++argv, --argc > 0) {
		opt.m_fragtype = argv[0];
                continue;
	    }
	}
	if (strcmp(arg, "-home") == 0) {
	    if (++argv, --argc > 0) {
		opt.m_home = argv[0];
		continue;
	    }
	}
	if (strcmp(arg, "-loop") == 0) {
	    if (++argv, --argc > 0) {
		if (str2num(arg, argv[0], &opt.m_loop))
		    continue;
	    }
	}
	if (strcmp(arg, "-nogetd") == 0) {
	    opt.m_nogetd = true;
	    continue;
	}
	if (strcmp(arg, "-noputd") == 0) {
	    opt.m_noputd = true;
	    continue;
	}
	if (strcmp(arg, "-nosort") == 0) {
	    opt.m_nosort = true;
	    continue;
	}
	if (strcmp(arg, "-scale") == 0) {
	    if (++argv, --argc > 0) {
		if (str2num(arg, argv[0], &opt.m_scale))
		    continue;
	    }
	}
	if (strcmp(arg, "-serial") == 0) {
	    opt.m_serial = true;
	    continue;
	}
	if (strcmp(arg, "-skip") == 0) {
	    if (++argv, --argc > 0) {
		assert(opt.m_skipcnt < arraySize(opt.m_skip));
		opt.m_skip[opt.m_skipcnt++] = argv[0];
		if (findCase(argv[0]))
		    continue;
	    }
	}
	if (strcmp(arg, "-subloop") == 0) {
	    if (++argv, --argc > 0) {
		if (str2num(arg, argv[0], &opt.m_subloop))
		    continue;
	    }
	}
	if (strcmp(arg, "-table") == 0) {
	    if (++argv, --argc > 0) {
		opt.m_table = argv[0];
		if (findTable())
		    continue;
	    }
	}
	if (strcmp(arg, "-threads") == 0) {
	    if (++argv, --argc > 0) {
		if (str2num(arg, argv[0], &opt.m_threads, 1, MAX_THR))
		    continue;
	    }
	}
	if (strcmp(arg, "-trace") == 0) {
	    if (++argv, --argc > 0) {
		if (str2num(arg, argv[0], &opt.m_trace))
		    continue;
	    }
	}
	if (strcmp(arg, "-v") == 0) {
	    if (++argv, --argc > 0) {
		if (str2num(arg, argv[0], &opt.m_v))
		    continue;
	    }
	}
	if (strncmp(arg, "-v", 2) == 0 && isdigit(arg[2])) {
	    if (str2num(arg, &arg[2], &opt.m_v))
		continue;
	}
	printusage();
	return NDBT_ProgramExit(NDBT_WRONGARGS);
    }
    homeEnv: {
	static char env[1000];
	if (opt.m_home != 0) {
	    sprintf(env, "NDB_HOME=%s", opt.m_home);
	    putenv(env);
	}
    }
    traceEnv: {
	static char env[40];
	sprintf(env, "NDB_ODBC_TRACE=%u", opt.m_trace);
	putenv(env);
    }
    debugEnv: {
	static char env[40];
	sprintf(env, "NDB_ODBC_DEBUG=%d", 1);
	putenv(env);
    }
    testMain();
    return NDBT_ProgramExit(NDBT_OK);
}

// vim: set sw=4:
