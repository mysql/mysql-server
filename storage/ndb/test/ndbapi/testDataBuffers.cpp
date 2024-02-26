/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

/*
 * testDataBuffers
 *
 * Test getValue() of byte arrays:
 * - using application buffers of different alignments and sizes
 * - using NdbApi allocated small (<32) and big (>=32) buffers
 *
 * Verifies fixes to tickets 189 and 206.
 *
 * Options: see printusage() below.
 *
 * Creates tables TB00 to TB15
 */

#include <ndb_global.h>
#include "portlib/ndb_compiler.h"

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbTest.hpp>
#include <NdbSchemaCon.hpp>
// limits
static int const MaxAttr = 64;
static int const MaxOper = 1000;
static int const MaxSize = 10000;
static int const MaxOff = 64;		// max offset to add to data buffer
static int const MaxData = MaxSize + MaxOff + 100;

// options
static int attrcnt = 25;
static int existok = 0;
static bool kontinue = false;
static int loopcnt = 1;
static int opercnt = 100;		// also does this many scans
static int randomizer = 171317;
static int sizelim = 500;
static int xverbose = 0;

static void printusage() {
    ndbout
	<< "usage: testDataBuffers options [default/max]"
	<< endl
	<< "NOTE: too large combinations result in NDB error"
	<< endl
	<< "-a N  number of attributes (including the key) [25/64]"
	<< endl
	<< "-e    no error if table exists (assumed to have same structure)"
	<< endl
	<< "-k    on error continue with next test case"
	<< endl
	<< "-l N  number of loops to run, 0 means infinite [1]"
	<< endl
	<< "-o N  number of operations (rows in each table) [100/1000]"
	<< endl
	<< "-r N  source of randomness (big number (prime)) [171317]"
	<< endl
	<< "-s N  array size limit (rounded up in some tests) [500/10000]"
	<< endl
	<< "-x    extremely verbose"
	<< endl
	<< "Tables: TB00 .. TB15"
	<< endl
	;
}

static Ndb* ndb = 0;
static NdbSchemaCon* tcon = 0;
static NdbSchemaOp* top = 0;
static NdbConnection* con = 0;
static NdbOperation* op = 0;
static NdbScanOperation* sop = 0;

static int
ndberror(char const* fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);

static int
ndberror(char const* fmt, ...)
{
    va_list ap;
    char buf[200];
    va_start(ap, fmt);
    BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ndbout << buf << " --" << endl;
    if (ndb)
      ndbout << "ndb : " << ndb->getNdbError() << endl;
    if (tcon)
	ndbout << "tcon: " << tcon->getNdbError() << endl;
    if (top)
	ndbout << "top: " << top->getNdbError() << endl;
    if (con)
	ndbout << "con : " << con->getNdbError() << endl;
    if (op)
	ndbout << "op  : " << op->getNdbError() << endl;
    return -1;
}

static int
chkerror(char const* fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);

static int
chkerror(char const* fmt, ...)
{
    va_list ap;
    char buf[200];
    va_start(ap, fmt);
    BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ndbout << "*** check failed: " << buf << " ***" << endl;
    return -1;
}

// alignment of addresses and data sizes

static bool isAligned(UintPtr x)
{
    return ((x & 3) == 0);
}
static bool isAligned(char* p)
{
    return isAligned(UintPtr(p));
}
static unsigned toAligned(unsigned x)
{
    while (! isAligned(x))
	x++;
    return x;
}
static char* toAligned(char* p)
{
    while (! isAligned(p))
	p++;
    return p;
}

// byte value for key k column i byte j
static int byteVal(int k, int i, int j)
{
    return '0' + (k + i + j)  % 10;
}

// tables

static char tab[20] = "";

static struct col {
    char aAttrName[20];
    AttrType aAttrType;
    int aAttrSize;
    int aArraySize;
    KeyType aTupleKey;
    bool nullable;
    NdbRecAttr* aRa;
    char* buf;
    int bufsiz;
    char data[MaxData];
} ccol[MaxAttr];

static int key = 0;

// independent test bits
static bool alignAddr;		// align our buffer addresses to 4x
static bool alignSize;		// align data sizes to 4x
static bool useBuf;		// use our buffers for output
static bool noRandom;		// do not randomize sizes and offsets
static int testbits = 4;

static int
makeSize(int i)
{
    int n;
    if (noRandom)
	n = i;
    else
	n = i * randomizer;
    n %= sizelim;
    if (n <= 0)
	n = 1;
    if (alignSize)
	n = toAligned(n);
    return n;
}

static int
makeOff(int k)
{
    int n;
    if (alignAddr)
	n = 0;
    else if (noRandom)
	n = k;
    else
	n = k * randomizer;
    n %= MaxOff;
    if (n < 0)
	n = -n;
    return n;
}

static int
testcase(Ndb_cluster_connection&cc, int flag)
{
    ndbout << "--- case " << flag << " ---" << endl;
    sprintf(tab, "TB%02d", flag);

    alignAddr = ! (flag & 1);
    ndbout << (alignAddr ? "align addresses" : "mis-align addresses") << endl;
    alignSize = ! (flag & 2);
    ndbout << (alignSize ? "align data sizes" : "mis-align data sizes") << endl;
    useBuf = ! (flag & 4);
    ndbout << (useBuf ? "use our buffers" : "use ndbapi buffers") << endl;
    noRandom = ! (flag & 8);
    ndbout << (noRandom ? "simple sizes" : "randomize sizes") << endl;

    int smax = 0, stot = 0, i;
    if (xverbose)
      ndbout << "- define table " << tab << endl;
    for (i = 0; i < attrcnt; i++) {
	col& c = ccol[i];
	memset(&c, 0, sizeof(c));
	sprintf(c.aAttrName, "C%d", i);
	if (i == 0) {
	    c.aAttrType = UnSigned;
	    c.aAttrSize = 32;
	    c.aArraySize = 1;
	    c.aTupleKey = TupleKey;
	    c.nullable = false;
	} else {
	    c.aAttrType = String;
	    c.aAttrSize = 8;
	    c.aArraySize = makeSize(i);
	    if (smax < c.aArraySize)
		smax = c.aArraySize;
	    stot += c.aArraySize;
	    c.aTupleKey = NoKey;
	    c.nullable = true;
	    if (xverbose)
	      ndbout << "-- column " << i << " size=" << c.aArraySize << endl;
	}
	c.buf = toAligned(c.data);
        c.bufsiz = (int)(sizeof(c.data) - (c.buf - c.data));
    }
    ndbout << "tab=" << tab << " cols=" << attrcnt
	<< " size max=" << smax << " tot=" << stot << endl;

    if ((tcon = NdbSchemaCon::startSchemaTrans(ndb)) == 0)
	return ndberror("startSchemaTransaction");
    if ((top = tcon->getNdbSchemaOp()) == 0)
	return ndberror("getNdbSchemaOp");
    if (top->createTable(tab) < 0)
	return ndberror("createTable");
    for (i = 0; i < attrcnt; i++) {
	col& c = ccol[i];
	if (top->createAttribute(
	    c.aAttrName,
	    c.aTupleKey,
	    c.aAttrSize,
	    c.aArraySize,
	    c.aAttrType,
	    MMBased,
	    c.nullable
	) < 0)
	    return ndberror("createAttribute col=%d", i);
    }
    if (tcon->execute() < 0) {
	if (! (tcon->getNdbError().code == 721 && existok))
	    return ndberror("execute");
	ndbout << "using " << tab << endl;
    } else {
	ndbout << "created " << tab << endl;
    }
    top = 0;
    tcon = 0;

    if (xverbose)
      ndbout << "- delete" << endl;
    int delcnt = 0;
    for (key = 0; key < opercnt; key++) {
	if ((con = ndb->startTransaction()) == 0)
	    return ndberror("startTransaction key=%d", key);
	if ((op = con->getNdbOperation(tab)) == 0)
	    return ndberror("getNdbOperation key=%d", key);
	if (op->deleteTuple() < 0)
	    return ndberror("deleteTuple key=%d", key);
	for (i = 0; i < attrcnt; i++) {
	    col& c = ccol[i];
	    if (i == 0) {
		if (op->equal(c.aAttrName, (char*)&key, sizeof(key)) < 0)
		    return ndberror("equal key=%d", key);
	    } else {
	    }
	}
	if (con->execute(Commit) < 0) {
	  if (con->getNdbError().code != 626)
	    return ndberror("execute key=%d", key);
	} else {
	    delcnt++;
	}
	ndb->closeTransaction(con);
    }
    con = 0;
    op = 0;
    ndbout << "deleted " << delcnt << endl;

    if (xverbose)
      ndbout << "- insert" << endl;
    for (key = 0; key < opercnt; key++) {
	int off = makeOff(key);
	if ((con = ndb->startTransaction()) == 0)
	    return ndberror("startTransaction key=%d", key);
	if ((op = con->getNdbOperation(tab)) == 0)
	    return ndberror("getNdbOperation key=%d", key);
	if (op->insertTuple() < 0)
	    return ndberror("insertTuple key=%d", key);
	for (i = 0; i < attrcnt; i++) {
	    col& c = ccol[i];
	    if (i == 0) {
		if (op->equal(c.aAttrName, (char*)&key, sizeof(key)) < 0)
		    return ndberror("equal key=%d", key);
	    } else {
		memset(c.buf, 'A', c.bufsiz);
		for (int j = 0; j < c.aArraySize; j++)
		    c.buf[j + off] = byteVal(key, i, j);
		if (op->setValue(c.aAttrName, c.buf + off, c.aArraySize) < 0)
		    return ndberror("setValue key=%d col=%d", key, i);
	    }
	}
	if (con->execute(Commit) < 0)
	    return ndberror("execute key=%d", key);
	ndb->closeTransaction(con);
    }
    con = 0;
    op = 0;
    ndbout << "inserted " << key << endl;

    if (xverbose)
      ndbout << "- select" << endl;
    for (key = 0; key < opercnt; key++) {
	int off = makeOff(key);
	if (xverbose)
	  ndbout << "-- key " << key << " off=" << off << endl;
	if ((con = ndb->startTransaction()) == 0)
	    return ndberror("startTransaction key=%d", key);
	if ((op = con->getNdbOperation(tab)) == 0)
	    return ndberror("getNdbOperation key=%d", key);
	if (op->readTuple() < 0)
	    return ndberror("readTuple key=%d", key);
	for (i = 0; i < attrcnt; i++) {
	    col& c = ccol[i];
	    if (i == 0) {
		if (op->equal(c.aAttrName, (char*)&key, sizeof(key)) < 0)
		    return ndberror("equal key=%d", key);
	    } else {
		if (xverbose) {
		  char tmp[20];
		  if (useBuf)
		    sprintf(tmp, "0x%p", c.buf + off);
		  else
		    strcpy(tmp, "ndbapi");
		  ndbout << "--- column " << i << " addr=" << tmp << endl;
		}
		memset(c.buf, 'B', c.bufsiz);
		if (useBuf) {
                    if (op->getValue(c.aAttrName, c.buf + off) == NULL)
			return ndberror("getValue key=%d col=%d", key, i);
		} else {
                    if ((c.aRa = op->getValue(c.aAttrName)) == NULL)
			return ndberror("getValue key=%d col=%d", key, i);
		}
	    }
	}
	if (con->execute(Commit) != 0)
	    return ndberror("execute key=%d", key);
	for (i = 0; i < attrcnt; i++) {
	    col& c = ccol[i];
	    if (i == 0) {
	    } else if (useBuf) {
                int j;
		for (j = 0; j < off; j++) {
		    if (c.buf[j] != 'B') {
			return chkerror("mismatch before key=%d col=%d pos=%d ok=%02x bad=%02x",
			    key, i, j, 'B', c.buf[j]);
		    }
		}
		for (j = 0; j < c.aArraySize; j++) {
		    if (c.buf[j + off] != byteVal(key, i, j)) {
			return chkerror("mismatch key=%d col=%d pos=%d ok=%02x bad=%02x",
			    key, i, j, byteVal(key, i, j), c.buf[j]);
		    }
		}
		for (j = c.aArraySize + off; j < c.bufsiz; j++) {
		    if (c.buf[j] != 'B') {
			return chkerror("mismatch after key=%d col=%d pos=%d ok=%02x bad=%02x",
			    key, i, j, 'B', c.buf[j]);
		    }
		}
	    } else {
		char* buf = c.aRa->aRef();
		if (buf == 0)
		    return ndberror("null aRef key=%d col%d", key, i);
		for (int j = 0; j < c.aArraySize; j++) {
		    if (buf[j] != byteVal(key, i, j)) {
			return chkerror("mismatch key=%d col=%d pos=%d ok=%02x bad=%02x",
			    key, i, j, byteVal(key, i, j), buf[j]);
		    }
		}
	    }
	}
	ndb->closeTransaction(con);
    }
    con = 0;
    op = 0;
    ndbout << "selected " << key << endl;

    if (xverbose)
      ndbout << "- scan" << endl;
    char found[MaxOper];
    int k;
    NdbDictionary::Dictionary * dict = ndb->getDictionary();
    const NdbDictionary::Table * table = dict->getTable(tab);

    for (k = 0; k < opercnt; k++)
	found[k] = 0;
    for (key = 0; key < opercnt; key++) {
	int off = makeOff(key);
        NdbInterpretedCode codeObj(table);
        NdbInterpretedCode *code= &codeObj;

	if (xverbose)
	  ndbout << "-- key " << key << " off=" << off << endl;
	int newkey = 0;
	if ((con = ndb->startTransaction()) == 0)
	    return ndberror("startTransaction key=%d", key);
	if ((op = sop = con->getNdbScanOperation(tab)) == 0)
	  return ndberror("getNdbOperation key=%d", key);
	if (sop->readTuples(1))
	  return ndberror("openScanRead key=%d", key);
	{
	    col& c = ccol[0];
            Uint32 colNum= table->getColumn(c.aAttrName)->getAttrId();
	    if (code->load_const_u32(1, key) < 0)
		return ndberror("load_const_u32");
	    if (code->read_attr(2, colNum) < 0)
		return ndberror("read_attr");
	    if (code->branch_eq(1, 2, 0) < 0)
		return ndberror("branch_eq");
	    if (code->interpret_exit_nok() < 0)
		return ndberror("interpret_exit_nok");
	    if (code->def_label(0) < 0)
		return ndberror("def_label");
	    if (code->interpret_exit_ok() < 0)
		return ndberror("interpret_exit_ok");
            if (code->finalise() != 0)
                return ndberror("finalise");
            if (sop->setInterpretedCode(code) != 0)
                return ndberror("setInterpretedCode");
	}
	for (i = 0; i < attrcnt; i++) {
	    col& c = ccol[i];
	    if (i == 0) {
                if (op->getValue(c.aAttrName, (char*)&newkey) == NULL)
		    return ndberror("getValue key=%d col=%d", key, i);
	    } else {
		if (xverbose) {
		  char tmp[20];
		  if (useBuf)
		    sprintf(tmp, "0x%p", c.buf + off);
		  else
		    strcpy(tmp, "ndbapi");
		  ndbout << "--- column " << i << " addr=" << tmp << endl;
		}
		memset(c.buf, 'C', c.bufsiz);
		if (useBuf) {
                    if (op->getValue(c.aAttrName, c.buf + off) == NULL)
			return ndberror("getValue key=%d col=%d", key, i);
		} else {
                    if ((c.aRa = op->getValue(c.aAttrName)) == NULL)
			return ndberror("getValue key=%d col=%d", key, i);
		}
	    }
	}
	if (con->execute(NoCommit) < 0)
	    return ndberror("executeScan key=%d", key);
	int ret, cnt = 0;
	while ((ret = sop->nextResult()) == 0) {
	    if (key != newkey)
		return ndberror("unexpected key=%d newkey=%d", key, newkey);
	    for (i = 1; i < attrcnt; i++) {
		col& c = ccol[i];
		if (useBuf) {
                    int j;
		    for (j = 0; j < off; j++) {
			if (c.buf[j] != 'C') {
			    return chkerror("mismatch before key=%d col=%d pos=%d ok=%02x bad=%02x",
				key, i, j, 'C', c.buf[j]);
			}
		    }
		    for (j = 0; j < c.aArraySize; j++) {
			if (c.buf[j + off] != byteVal(key, i, j)) {
			    return chkerror("mismatch key=%d col=%d pos=%d ok=%02x bad=%02x",
				key, i, j, byteVal(key, i, j), c.buf[j]);
			}
		    }
		    for (j = c.aArraySize + off; j < c.bufsiz; j++) {
			if (c.buf[j] != 'C') {
			    return chkerror("mismatch after key=%d col=%d pos=%d ok=%02x bad=%02x",
				key, i, j, 'C', c.buf[j]);
			}
		    }
		} else {
		    char* buf = c.aRa->aRef();
		    if (buf == 0)
			return ndberror("null aRef key=%d col%d", key, i);
		    for (int j = 0; j < c.aArraySize; j++) {
			if (buf[j] != byteVal(key, i, j)) {
			    return chkerror("mismatch key=%d col=%d pos=%d ok=%02x bad=%02x",
				key, i, j, byteVal(key, i, j), buf[j]);
			}
		    }
		}
	    }
	    cnt++;
	}
	if (ret < 0)
	    return ndberror("nextScanResult key=%d", key);
	if (cnt != 1)
	    return ndberror("scan key=%d found %d", key, cnt);
	found[key] = 1;
	ndb->closeTransaction(con);
    }
    con = 0;
    op = 0;
    for (k = 0; k < opercnt; k++)
	if (! found[k])
	    return ndberror("key %d not found", k);
    ndbout << "scanned " << key << endl;

    ndbout << "done" << endl;
    return 0;
}

int main(int argc, char** argv)
{
    int i;
    ndb_init();
    while (++argv, --argc > 0) {
	char const* p = argv[0];
	if (*p++ != '-' || strlen(p) != 1)
	    goto wrongargs;
	switch (*p) {
	case 'a':
	    if (++argv, --argc > 0) {
		attrcnt = atoi(argv[0]);
		if (1 <= attrcnt && attrcnt <= MaxAttr)
		    break;
	    }
	    goto wrongargs;
	case 'e':
	    existok = 1;
	    break;
	case 'k':
	    kontinue = true;
	    break;
	case 'l':
	    if (++argv, --argc > 0) {
		loopcnt = atoi(argv[0]);
		if (0 <= loopcnt)
		    break;
	    }
	    goto wrongargs;
	case 'o':
	    if (++argv, --argc > 0) {
		opercnt = atoi(argv[0]);
		if (0 <= opercnt && opercnt <= MaxOper)
		    break;
	    }
	    goto wrongargs;
	case 'r':
	    if (++argv, --argc > 0) {
		randomizer = atoi(argv[0]);
		if (1 <= randomizer)
		    break;
	    }
	    goto wrongargs;
	case 's':
	    if (++argv, --argc > 0) {
		sizelim = atoi(argv[0]);
		if (1 <= sizelim && sizelim <= MaxSize)
		    break;
	    }
	    goto wrongargs;
	case 'x':
	    xverbose = 1;
	    break;
	default:
	wrongargs:
	    printusage();
	    return NDBT_ProgramExit(NDBT_WRONGARGS);
	}
    }

    unsigned ok = true;

    Ndb_cluster_connection con;
    if(con.connect(12, 5, 1))
    {
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    ndb = new Ndb(&con, "TEST_DB");
    if (ndb->init() != 0)
    {
	ndberror("init");
	ok = false;
	goto out;
    }
    if (ndb->waitUntilReady(30) < 0)
    {
      ndberror("waitUntilReady");
      ok = false;
      goto out;
    }
    
    for (i = 1; 0 == loopcnt || i <= loopcnt; i++) {
	ndbout << "=== loop " << i << " ===" << endl;
	for (int flag = 0; flag < (1<<testbits); flag++) {
	    if (testcase(con, flag) < 0) {
		ok = false;
		if (! kontinue)
		    goto out;
	    }
	    NdbDictionary::Dictionary * dict = ndb->getDictionary();
	    dict->dropTable(tab);
	}
    }
    
out:
    delete ndb;
    return NDBT_ProgramExit(ok ? NDBT_OK : NDBT_FAILED);
}

// vim: set sw=4:
