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
#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <NdbTest.hpp>
#include <NdbTick.h>
#include <ndb_limits.h>

struct Opt {
    bool m_core;
    const char* m_table;
    Opt() :
	m_core(false),
    	m_table("TB1")
    {
    }
};

static Opt opt;

static void printusage()
{
    Opt d;
    ndbout
	<< "usage: testBlobs [options]" << endl
	<< "-core       dump core on error - default " << d.m_core << endl
	;
}

static Ndb* myNdb = 0;
static NdbDictionary::Dictionary* myDic = 0;
static NdbConnection* myCon = 0;
static NdbOperation* myOp = 0;
static NdbBlob* myBlob = 0;

static void
fatal(const char* fmt, ...)
{
    va_list ap;
    char buf[200];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ndbout << "fatal: " << buf << endl;
    if (myNdb != 0 && myNdb->getNdbError().code != 0)
	ndbout << "ndb - " << myNdb->getNdbError() << endl;
    if (myDic != 0 && myDic->getNdbError().code != 0)
	ndbout << "dic - " << myDic->getNdbError() << endl;
    if (opt.m_core)
	abort();
    NDBT_ProgramExit(NDBT_FAILED);
    exit(1);
}

static void
dropBlobsTable()
{
    NdbDictionary::Table tab(NDB_BLOB_TABLE_NAME);
    if (myDic->dropTable(tab) == -1)
	if (myDic->getNdbError().code != 709)
	    fatal("dropTable");
}

static void
createBlobsTable()
{
    NdbDictionary::Table tab(NDB_BLOB_TABLE_NAME);
    // col 0
    NdbDictionary::Column col0("BLOBID");
    col0.setPrimaryKey(true);
    col0.setType(NdbDictionary::Column::Bigunsigned);
    tab.addColumn(col0);
    // col 1
    NdbDictionary::Column col1("DATA");
    col1.setPrimaryKey(false);
    col1.setType(NdbDictionary::Column::Binary);
    col1.setLength(NDB_BLOB_PIECE_SIZE);
    tab.addColumn(col1);
    // create
    if (myDic->createTable(tab) == -1)
	fatal("createTable");
}

static void
dropTable()
{
    NdbDictionary::Table tab(opt.m_table);
    if (myDic->dropTable(tab) == -1)
	if (myDic->getNdbError().code != 709)
	    fatal("dropTable");
}

static void
createTable()
{
    NdbDictionary::Table tab(opt.m_table);
    // col 0
    NdbDictionary::Column col0("A");
    col0.setPrimaryKey(true);
    col0.setType(NdbDictionary::Column::Unsigned);
    tab.addColumn(col0);
    // col 1
    NdbDictionary::Column col1("B");
    col1.setPrimaryKey(false);
    col1.setType(NdbDictionary::Column::Blob);
    tab.addColumn(col1);
    // create
    if (myDic->createTable(tab) == -1)
	fatal("createTable");
}

static void
insertData(Uint32 key)
{
}

static void
insertTuples()
{
    for (Uint32 key = 0; key <= 99; key++) {
	if ((myCon = myNdb->startTransaction()) == 0)
	    fatal("startTransaction");
	if ((myOp = myCon->getNdbOperation(opt.m_table)) == 0)
	    fatal("getNdbOperation");
	if (myOp->insertTuple() == -1)
	    fatal("insertTuple");
	if (myOp->setValue((unsigned)0, key) == -1)
	    fatal("setValue %u", (unsigned)key);
	if ((myBlob = myOp->setBlob(1)) == 0)
	    fatal("setBlob");
	if (myCon->execute(NoCommit) == -1)
	    fatal("execute NoCommit");
	insertData(key);
	if (myCon->execute(Commit) == -1)
	    fatal("execute Commit");
	myNdb->closeTransaction(myCon);
	myOp = 0;
	myBlob = 0;
	myCon = 0;
    }
}

static void
testMain()
{
    myNdb = new Ndb("TEST_DB");
    if (myNdb->init() != 0)
	fatal("init");
    if (myNdb->waitUntilReady() < 0)
	fatal("waitUntilReady");
    myDic = myNdb->getDictionary();
    dropBlobsTable();
    createBlobsTable();		// until moved to Ndbcntr
    dropTable();
    createTable();
    insertTuples();
}

NDB_COMMAND(testOdbcDriver, "testBlobs", "testBlobs", "testBlobs", 65535)
{
    while (++argv, --argc > 0) {
	const char* arg = argv[0];
	if (strcmp(arg, "-core") == 0) {
	    opt.m_core = true;
	    continue;
	}
    }
    testMain();
    return NDBT_ProgramExit(NDBT_OK);
}

// vim: set sw=4:
