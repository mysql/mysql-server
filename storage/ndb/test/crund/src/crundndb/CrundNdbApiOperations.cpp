/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>
#include <cassert>

#include <NdbApi.hpp>
#include <NdbError.hpp>

#include "helpers.hpp"
#include "string_helpers.hpp"

#include "CrundNdbApiOperations.hpp"

//using namespace std;
using std::cout;
using std::flush;
using std::endl;
using std::string;
using std::vector;

// JNI crashes with gcc & operator<<(ostream &, long/int)
using utils::toString;

/************************************************************
 * Helper Macros & Functions
 ************************************************************/

// This benchmark's error handling of NDBAPI calls is rigorous but crude:
// - all calls' return value is checked for errors
// - all errors are reported and then followed by a process exit

/*
// JNI crashes with gcc & operator<<(ostream &, long/int)
#define ABORT_NDB_ERROR0(error)                                         \
    do { cout << "!!! error in " << __FILE__ << ", line: " << __LINE__  \
              << ", code: " << (int)error.code                          \
              << ", msg: " << error.message << "." << endl;             \
        exit(-1);                                                       \
    } while (0)
*/
#define ABORT_NDB_ERROR(error)                                          \
    do {                                                                \
        char l[1024];                                                   \
        sprintf(l, "%d", __LINE__);                                     \
        char c[1024];                                                   \
        sprintf(c, "%d", error.code);                                   \
        cout << endl << "!!! error in " << __FILE__                     \
             << ", line: " << l << "," << endl;                         \
        cout << "    error code: " << c                                 \
             << ", error msg: " << error.message << "." << endl;        \
        exit(-1);                                                       \
    } while (0)

#define VERIFY(cond)                                                    \
    if (cond); else ABORT_ERROR("wrong data; verification failed")

/************************************************************
 * Member Functions of Class CrundModel
 ************************************************************/

void
CrundModel::init(Ndb* ndb)
{
    const NdbDictionary::Dictionary* dict = ndb->getDictionary();

    // get columns of table A
    if ((table_A = dict->getTable("a")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_A_id = table_A->getColumn("id")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_A_cint = table_A->getColumn("cint")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_A_clong = table_A->getColumn("clong")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_A_cfloat = table_A->getColumn("cfloat")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_A_cdouble = table_A->getColumn("cdouble")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());

    // get columns of table B0
    if ((table_B0 = dict->getTable("b0")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_B0_id = table_B0->getColumn("id")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_B0_cint = table_B0->getColumn("cint")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_B0_clong = table_B0->getColumn("clong")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_B0_cfloat = table_B0->getColumn("cfloat")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_B0_cdouble = table_B0->getColumn("cdouble")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_B0_a_id = table_B0->getColumn("a_id")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_B0_cvarbinary_def = table_B0->getColumn("cvarbinary_def")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_B0_cvarchar_def = table_B0->getColumn("cvarchar_def")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());

    // get indexes of table B0
    if ((idx_B0_a_id = dict->getIndex("I_B0_FK", "b0")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());

    // get common attribute ids for tables A, B0
    attr_id = column_A_id->getAttrId();
    if (attr_id != column_B0_id->getAttrId())
        ABORT_ERROR("attribute id mismatch");
    attr_cint = column_A_cint->getAttrId();
    if (attr_cint != column_B0_cint->getAttrId())
        ABORT_ERROR("attribute id mismatch");
    attr_clong = column_A_clong->getAttrId();
    if (attr_clong != column_B0_clong->getAttrId())
        ABORT_ERROR("attribute id mismatch");
    attr_cfloat = column_A_cfloat->getAttrId();
    if (attr_cfloat != column_B0_cfloat->getAttrId())
        ABORT_ERROR("attribute id mismatch");
    attr_cdouble = column_A_cdouble->getAttrId();
    if (attr_cdouble != column_B0_cdouble->getAttrId())
        ABORT_ERROR("attribute id mismatch");

    // get attribute ids for table B0
    attr_B0_a_id = column_B0_a_id->getAttrId();
    attr_B0_cvarbinary_def = column_B0_cvarbinary_def->getAttrId();
    attr_B0_cvarchar_def = column_B0_cvarchar_def->getAttrId();

    // get attribute ids for columns in index B0_a_id
    attr_idx_B0_a_id = idx_B0_a_id->getColumn(0)->getAttrId();
}

/************************************************************
 * Member Functions of Class Operations
 ************************************************************/

/* XXX 5.1 Reference Manual, 16.14.3

 Transaction isolation level. The NDBCLUSTER storage engine supports only
 the READ COMMITTED transaction isolation level.

 If a SELECT from a Cluster table includes a BLOB or TEXT column, the
 READ COMMITTED transaction isolation level is converted to a read with
 read lock. This is done to guarantee consistency, due to the fact that
 parts of the values stored in columns of these types are actually read
 from a separate table.

 DELETE FROM (even with no WHERE clause) is transactional. For tables
 containing a great many rows, you may find that performance is improved
 by using several DELETE FROM ... LIMIT ... statements to “chunk” the
 delete operation. If your objective is to empty the table, then you may
 wish to use TRUNCATE instead.
*/

void
CrundNdbApiOperations::init(const char* mgmd_conn_str)
{
    assert(mgmd == NULL);
    assert(mgmd_conn_str);

    // ndb_init must be called first
    cout << endl
         << "initializing NDBAPI ..." << flush;
    int stat = ndb_init();
    if (stat != 0)
        ABORT_ERROR("ndb_init() returned: " << stat);
    cout << "         [ok]" << endl;

    // instantiate NDB cluster singleton
    cout << "creating cluster connection ..." << flush;
    assert(mgmd_conn_str);
    mgmd = new Ndb_cluster_connection(mgmd_conn_str);
    cout << " [ok]" << endl; // no useful mgmd->string conversion

    // connect to cluster management node (ndb_mgmd)
    cout << "connecting to mgmd ..." << flush;
    const int retries = 0; // number of retries (< 0 = indefinitely)
    const int delay = 0;   // seconds to wait after retry
    const int verbose = 1; // print report of progess
    // returns: 0 = success, 1 = recoverable error, -1 = non-recoverable error
    if (mgmd->connect(retries, delay, verbose) != 0)
        ABORT_ERROR("mgmd@" << mgmd_conn_str << " was not ready within "
                     << (retries * delay) << "s.");
    cout << "          [ok: " << mgmd_conn_str << "]" << endl;
}

void
CrundNdbApiOperations::close()
{
    assert(mgmd != NULL);

    cout << "closing cluster connection ..." << flush;
    delete mgmd;
    mgmd = NULL;
    cout << "  [ok]" << endl;

    // ndb_close must be called last
    cout << "closing NDBAPI ...   " << flush;
    ndb_end(0);
    cout << "           [ok]" << endl;
}

void
CrundNdbApiOperations::initConnection(const char* catalog, const char* schema,
                                      NdbOperation::LockMode defaultLockMode)
{
    assert(mgmd != NULL);
    assert(ndb == NULL);
    assert(tx == NULL);
    assert(model == NULL);

    // optionally, connect and wait for reaching the data nodes (ndbds)
    cout << "waiting for data nodes ..." << flush;
    const int initial_wait = 10; // seconds to wait until first node detected
    const int final_wait = 0;    // seconds to wait after first node detected
    // returns: 0 all nodes live, > 0 at least one node live, < 0 error
    if (mgmd->wait_until_ready(initial_wait, final_wait) < 0)
        ABORT_ERROR("data nodes were not ready within "
                     << (initial_wait + final_wait) << "s.");
    cout << "      [ok]" << endl;

    // connect to database
    cout << "connecting to database ..." << flush;
    ndb = new Ndb(mgmd, catalog, schema);
    const int max_no_tx = 10; // maximum number of parallel tx (<=1024)
    // note each scan or index scan operation uses one extra transaction
    //if (ndb->init() != 0)
    if (ndb->init(max_no_tx) != 0)
        ABORT_NDB_ERROR(ndb->getNdbError());
    cout << "      [ok: " << catalog << "." << schema << "]" << endl;

    cout << "caching metadata ..." << flush;
    CrundModel* m = new CrundModel();
    m->init(ndb);
    model = m;
    cout << "            [ok]" << endl;

    cout << "using lock mode for reads ..." << flush;
    ndbOpLockMode = defaultLockMode;
    string lm;
    switch (defaultLockMode) {
    case NdbOperation::LM_CommittedRead:
        lm = "LM_CommittedRead";
        break;
    case NdbOperation::LM_Read:
        lm = "LM_Read";
        break;
    case NdbOperation::LM_Exclusive:
        lm = "LM_Exclusive";
        break;
    default:
        ndbOpLockMode = NdbOperation::LM_CommittedRead;
        lm = "LM_CommittedRead";
        assert(false);
    }
    cout << "   [ok: " + lm + "]" << endl;
}

void
CrundNdbApiOperations::closeConnection()
{
    assert(mgmd != NULL);
    assert(ndb != NULL);
    assert(tx == NULL);
    assert(model != NULL);

    cout << "clearing metadata cache ..." << flush;
    delete model;
    model = NULL;
    cout << "     [ok]" << endl;

    cout << "closing database connection ..." << flush;
    // no ndb->close();
    delete ndb;
    ndb = NULL;
    cout << " [ok]" << endl;
}

void
CrundNdbApiOperations::beginTransaction()
{
    assert(tx == NULL);

    // start a transaction
    // must be closed with Ndb::closeTransaction or NdbTransaction::close
    if ((tx = ndb->startTransaction()) == NULL)
        ABORT_NDB_ERROR(ndb->getNdbError());
}

void
CrundNdbApiOperations::executeOperations()
{
    assert(tx != NULL);

    // execute but don't commit the current transaction
    if (tx->execute(NdbTransaction::NoCommit) != 0
        || tx->getNdbError().status != NdbError::Success)
        ABORT_NDB_ERROR(tx->getNdbError());
}

void
CrundNdbApiOperations::commitTransaction()
{
    assert(tx != NULL);

    // commit the current transaction
    if (tx->execute(NdbTransaction::Commit) != 0
        || tx->getNdbError().status != NdbError::Success)
        ABORT_NDB_ERROR(tx->getNdbError());
}

void
CrundNdbApiOperations::closeTransaction()
{
    assert(tx != NULL);

    // close the current transaction
    // to be called irrespectively of success or failure
    ndb->closeTransaction(tx);
    tx = NULL;
}

// ----------------------------------------------------------------------

void
CrundNdbApiOperations::clearData()
{
    cout << "deleting all rows ..." << flush;
    const bool batch = true;
    int delB0 = -1;
    delByScan(model->table_B0, delB0, batch);
    cout << "           [B0: " << toString(delB0) << flush;
    int delA = -1;
    delByScan(model->table_A, delA, batch);
    cout << ", A: " << toString(delA) << "]" << endl;
}

struct CommonAB {
    Int32 id;
    Int32 cint;
    Int64 clong;
    float cfloat;
    double cdouble;
};

// for sorting
static inline bool
compare(CommonAB i,CommonAB j) {
    return (i.id < j.id);
}

static inline Int32
getCommonAB(const CommonAB* const ab)
{
    Int32 cint = ab->cint;
    Int64 clong = ab->clong;
    VERIFY(clong == cint);
    float cfloat = ab->cfloat;
    VERIFY(cfloat == cint);
    double cdouble = ab->cdouble;
    VERIFY(cdouble == cint);
    return cint;
}

// some string literals
static const char* const astring1 = "i";
static const char* const astring10 = "xxxxxxxxxx";
static const char* const astring100 = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
static const char* const astring1000 = "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm";

static inline const char*
selectString(int length)
{
    switch (length) {
    case 0: return NULL;
    case 1: return astring1;
    case 10: return astring10;
    case 100: return astring100;
    case 1000: return astring1000;
    default:
        assert(false);
        return "";
    }
}

void
CrundNdbApiOperations::ins(const NdbDictionary::Table* table,
                           int from, int to,
                           bool setAttrs, bool batch)
{
    beginTransaction();
    for (int i = from; i <= to; i++) {
        // get an insert operation for the table
        NdbOperation* op = tx->getNdbOperation(table);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->insertTuple() != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set values; key attribute needs to be set first
        if (op->equal(model->attr_id, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (setAttrs) {
            if (op->setValue(model->attr_cint, (Int32)-i) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op->setValue(model->attr_clong, (Int64)-i) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op->setValue(model->attr_cfloat, (float)-i) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op->setValue(model->attr_cdouble, (double)-i) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());
        }

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();
}

void
CrundNdbApiOperations::delByScan(const NdbDictionary::Table* table, int& count,
                                 bool batch)
{
    beginTransaction();

    // get a full table scan operation (no scan filter defined)
    NdbScanOperation* op = tx->getNdbScanOperation(table);
    if (op == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());

    // define a read scan with exclusive locks
    const NdbOperation::LockMode lock_mode = NdbOperation::LM_Exclusive;
    const int scan_flags = 0;
    const int parallel = 0;
    const int batch_ = 0;
    if (op->readTuples(lock_mode, scan_flags, parallel, batch_) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());

    // start the scan; don't commit yet
    executeOperations();

    // delete all rows in a given scan
    count = 0;
    int stat;
    const bool allowFetch = true; // request new batches when exhausted
    const bool forceSend = false; // send may be delayed
    while ((stat = op->nextResult(allowFetch, forceSend)) == 0) {
        // delete all tuples within a batch
        do {
            if (op->deleteCurrentTuple() != 0)
                ABORT_NDB_ERROR(tx->getNdbError());
            count++;

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        } while ((stat = op->nextResult(!allowFetch, forceSend)) == 0);

        if (stat == 1) {
            // no more batches
            break;
        }
        if (stat == 2) {
            // end of current batch, fetch next
            if (tx->execute(NdbTransaction::NoCommit) != 0
                || tx->getNdbError().status != NdbError::Success)
                ABORT_NDB_ERROR(tx->getNdbError());
            continue;
        }
        ABORT_ERROR("stat == " + stat);
    }
    if (stat != 1)
        ABORT_ERROR("stat == " + stat);

    // close the scan
    const bool forceSend_ = false;
    const bool releaseOp = false;
    op->close(forceSend_, releaseOp);
    //CDBG << "!!! deleted " << toString(count) << " rows" << endl;

    commitTransaction();
    closeTransaction();
}

void
CrundNdbApiOperations::delByPK(const NdbDictionary::Table* table,
                               int from, int to,
                               bool batch)
{
    beginTransaction();
    for (int i = from; i <= to; i++) {
        // get a delete operation for the table
        NdbOperation* op = tx->getNdbOperation(table);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->deleteTuple() != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set key attribute
        if (op->equal(model->attr_id, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();
}

void
CrundNdbApiOperations::setByPK(const NdbDictionary::Table* table,
                    int from, int to,
                    bool batch)
{
    beginTransaction();
    for (int i = from; i <= to; i++) {
        // get an update operation for the table
        NdbOperation* op = tx->getNdbOperation(table);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->updateTuple() != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set values; key attribute needs to be set first
        if (op->equal(model->attr_id, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->setValue(model->attr_cint, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->setValue(model->attr_clong, (Int64)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->setValue(model->attr_cfloat, (float)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->setValue(model->attr_cdouble, (double)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();
}

void
CrundNdbApiOperations::getByPK_bb(const NdbDictionary::Table* table,
                       int from, int to,
                       bool batch)
{
    // allocate attributes holder
    const int count = (to - from) + 1;
    CommonAB* const ab = new CommonAB[count];

    // fetch attributes by key
    beginTransaction();
    CommonAB* pab = ab;
    for (int i = from; i <= to; i++, pab++) {
        // get a read operation for the table
        NdbOperation* op = tx->getNdbOperation(table);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->readTuple(ndbOpLockMode) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set key attribute
        if (op->equal(model->attr_id, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // get attributes (not readable until after commit)
        if (op->getValue(model->attr_id, (char*)&pab->id) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_cint, (char*)&pab->cint) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_clong, (char*)&pab->clong) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_cfloat, (char*)&pab->cfloat) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_cdouble, (char*)&pab->cdouble) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();

    // check fetched values
    pab = ab;
    for (int i = from; i <= to; i++, pab++) {
        // check fetched values
        Int32 id = pab->id;
        VERIFY(id == i);

        Int32 j = getCommonAB(pab);
        //CDBG << "!!! id=" << toString(id) << ", i=" << toString(i) << endl;
        VERIFY(j == id);
    }

    // release attributes holder
    delete[] ab;
}

struct CommonAB_AR {
    NdbRecAttr* id;
    NdbRecAttr* cint;
    NdbRecAttr* clong;
    NdbRecAttr* cfloat;
    NdbRecAttr* cdouble;
};

static inline Int32
getCommonAB(const CommonAB_AR* const ab)
{
    Int32 cint = ab->cint->int32_value();
    Int64 clong = ab->clong->int64_value();
    VERIFY(clong == cint);
    float cfloat = ab->cfloat->float_value();
    VERIFY(cfloat == cint);
    double cdouble = ab->cdouble->double_value();
    VERIFY(cdouble == cint);
    return cint;
}

void
CrundNdbApiOperations::getByPK_ar(const NdbDictionary::Table* table,
                       int from, int to,
                       bool batch)
{
    // allocate attributes holder
    const int count = (to - from) + 1;
    CommonAB_AR* const ab = new CommonAB_AR[count];

    // fetch attributes by key
    beginTransaction();
    CommonAB_AR* pab = ab;
    for (int i = from; i <= to; i++, pab++) {
        // get a read operation for the table
        NdbOperation* op = tx->getNdbOperation(table);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->readTuple(ndbOpLockMode) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set key attribute
        if (op->equal(model->attr_id, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // get attributes (not readable until after commit)
        if ((pab->id = op->getValue(model->attr_id, NULL)) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if ((pab->cint = op->getValue(model->attr_cint, NULL)) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if ((pab->clong = op->getValue(model->attr_clong, NULL)) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if ((pab->cfloat = op->getValue(model->attr_cfloat, NULL)) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if ((pab->cdouble = op->getValue(model->attr_cdouble, NULL)) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();

    // check fetched values
    pab = ab;
    for (int i = from; i <= to; i++, pab++) {
        // check fetched values
        Int32 id = pab->id->int32_value();
        VERIFY(id == i);

        Int32 j = getCommonAB(pab);
        //CDBG << "!!! id=" << toString(id) << ", i=" << toString(i) << endl;
        VERIFY(j == id);
    }

    // release attributes holder
    delete[] ab;
}

void
CrundNdbApiOperations::setVarbinary(const NdbDictionary::Table* table,
                         int from, int to, bool batch, int length)
{
    setVar(table, model->attr_B0_cvarbinary_def,
           from, to, batch, selectString(length));
}

void
CrundNdbApiOperations::setVarchar(const NdbDictionary::Table* table,
                       int from, int to, bool batch, int length)
{
    setVar(table, model->attr_B0_cvarchar_def,
           from, to, batch, selectString(length));
}

void
CrundNdbApiOperations::getVarbinary(const NdbDictionary::Table* table,
                         int from, int to, bool batch, int length)
{
    getVar(table, model->attr_B0_cvarbinary_def,
           from, to, batch, selectString(length));
}

void
CrundNdbApiOperations::getVarchar(const NdbDictionary::Table* table,
                       int from, int to, bool batch, int length)
{
    getVar(table, model->attr_B0_cvarchar_def,
           from, to, batch, selectString(length));
}

void
CrundNdbApiOperations::setVar(const NdbDictionary::Table* table, int attr_cvar,
                   int from, int to,
                   bool batch, const char* str)
{
    char* buf = NULL;
    if (str != NULL) {
        // allocate attributes holder
        size_t slen = strlen(str);
        // XXX assumes column declared as VARBINARY/CHAR(<255)
        size_t sbuf = 1 + slen;
        // XXX buffer overflow if slen >255!!!
        assert(slen < 255);
        buf = new char[sbuf];
        buf[0] = (char)slen;
        memcpy(buf + 1, str, slen);
        //CDBG << "!!! buf[0]=" << toString(buf[0]) << endl;
        //CDBG << "!!! buf[1]=" << toString(buf[1]) << endl;
        //CDBG << "!!! buf[" << toString(slen) << "]=" << toString(buf[slen]) << endl;
    }

    beginTransaction();
    for (int i = from; i <= to; i++) {
        // get an update operation for the table
        NdbOperation* op = tx->getNdbOperation(table);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->updateTuple() != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set values; key attribute needs to be set first
        if (op->equal(model->attr_id, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->setValue(attr_cvar, buf) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();

    // release attributes holder
    if (buf != NULL) {
        delete[] buf;
    }
}

void
CrundNdbApiOperations::getVar(const NdbDictionary::Table* table, int attr_cvar,
                   int from, int to,
                   bool batch, const char* str)
{
    assert(str);

    // allocate attributes holder
    const int count = (to - from) + 1;
    size_t slen = strlen(str);
    const size_t sline = (1 + slen);
    const size_t sbuf = count * sline;
    char* const buf = new char[sbuf];
    //memset(buf, 1, sbuf);
    //CDBG << "!!! buf[0]=" << toString(buf[0]) << endl;
    //CDBG << "!!! buf[1]=" << toString(buf[1]) << endl;
    //CDBG << "!!! buf[" << toString(slen) << "]=" << toString(buf[slen]) << endl;
    //CDBG << "!!! buf[" << toString(slen+1) << "]=" << toString(buf[slen+1]) << endl;

    // fetch string attribute by key
    char* s = buf;
    beginTransaction();
    for (int i = from; i <= to; i++, s += sline) {
        // get a read operation for the table
        NdbOperation* op = tx->getNdbOperation(table);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->readTuple(ndbOpLockMode) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set key attribute
        if (op->equal(model->attr_id, (Int32)1) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // get attributes (not readable until after commit)
        if (op->getValue(attr_cvar, (char*)s) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();
    assert(s == buf + sbuf);

    // copy (move) the strings to make them aligned and 0-terminated
    s = buf;
    for (int i = from; i <= to; i++, s += sline) {
        //CDBG << "!!! s[0]=" << toString(s[0]) << endl;
        //CDBG << "!!! s[1]=" << toString(s[1]) << endl;
        //CDBG << "!!! s[" << toString(slen) << "]=" << toString(s[slen]) << endl;
        //CDBG << "!!! s[" << toString(slen+1) << "]=" << toString(s[slen + 1]) << endl;

        const size_t n = s[0];
        VERIFY(n < sline);

        // move and 0-terminated string
        memmove(s, s + 1, n);
        s[n] = 0;

        // check fetched values
        VERIFY(strcmp(s, str) == 0);
    }
    assert(s == buf + sbuf);

    // release attributes holder
    delete[] buf;
}

void
CrundNdbApiOperations::setB0ToA(int nOps, bool batch)
{
    beginTransaction();
    for (int i = 1; i <= nOps; i++) {
        // get an update operation for the table
        NdbOperation* op = tx->getNdbOperation(model->table_B0);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->updateTuple() != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set key attribute
        if (op->equal(model->attr_id, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set a_id attribute
        int a_id = ((i - 1) % nOps) + 1;
        if (op->setValue(model->attr_B0_a_id, (Int32)a_id) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();
}

void
CrundNdbApiOperations::nullB0ToA(int nOps, bool batch)
{
    beginTransaction();
    for (int i = 1; i <= nOps; i++) {
        // get an update operation for the table
        NdbOperation* op = tx->getNdbOperation(model->table_B0);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->updateTuple() != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set key attribute
        if (op->equal(model->attr_id, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // clear a_id attribute
        if (op->setValue(model->attr_B0_a_id, (char*)NULL) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();
}

void
CrundNdbApiOperations::navB0ToA(int nOps, bool batch)
{
    // allocate attributes holder
    CommonAB* const ab = new CommonAB[nOps];

    // fetch the foreign keys from B0 and read attributes from A
    beginTransaction();
    CommonAB* pab = ab;
    for (int i = 1; i <= nOps; i++, pab++) {
        // fetch the foreign key value from B0
        Int32 a_id;
        {
            // get a read operation for the table
            NdbOperation* op = tx->getNdbOperation(model->table_B0);
            if (op == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op->readTuple(ndbOpLockMode) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());

            // set key attribute
            if (op->equal(model->attr_id, (Int32)i) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());

            // get attribute (not readable until after commit)
            if (op->getValue(model->attr_B0_a_id, (char*)&a_id) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
        }
        executeOperations(); // execute the operation; don't commit yet

        // fetch the attributes from A
        {
            // get a read operation for the table
            NdbOperation* op = tx->getNdbOperation(model->table_A);
            if (op == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op->readTuple(ndbOpLockMode) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());

            // set key attribute
            assert(a_id == ((i - 1) % nOps) + 1);
            if (op->equal(model->attr_id, a_id) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());

            // get attributes (not readable until after commit)
            if (op->getValue(model->attr_id, (char*)&pab->id) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op->getValue(model->attr_cint, (char*)&pab->cint) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op->getValue(model->attr_clong, (char*)&pab->clong) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op->getValue(model->attr_cfloat, (char*)&pab->cfloat) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op->getValue(model->attr_cdouble, (char*)&pab->cdouble) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
        }

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();

    // check fetched values
    pab = ab;
    for (int i = 1; i <= nOps; i++, pab++) {
        // check fetched values
        Int32 id = pab->id;
        VERIFY(id == ((i - 1) % nOps) + 1);

        Int32 j = getCommonAB(pab);
        //CDBG << "!!! id=" << toString(id) << ", i=" << toString(i) << endl;
        VERIFY(j == id);
    }

    // release attributes holder
    delete[] ab;
}

void
CrundNdbApiOperations::navB0ToAalt(int nOps, bool batch)
{
    // allocate foreign key values holder
    Int32* const a_id = new Int32[nOps];

    // fetch the foreign key values from B0
    beginTransaction();
    Int32* pa_id = a_id;
    for (int i = 1; i <= nOps; i++) {
        // get a read operation for the table
        NdbOperation* op = tx->getNdbOperation(model->table_B0);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->readTuple(ndbOpLockMode) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set key attribute
        if (op->equal(model->attr_id, (Int32)i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // get attribute (not readable until after commit)
        if (op->getValue(model->attr_B0_a_id, (char*)pa_id++) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    executeOperations(); // execute the operation; don't commit yet

    // allocate attributes holder
    CommonAB* const ab = new CommonAB[nOps];

    // fetch rows from A
    pa_id = a_id;
    CommonAB* pab = ab;
    for (int i = 1; i <= nOps; i++, pa_id++, pab++) {
        // get a read operation for the table
        NdbOperation* op = tx->getNdbOperation(model->table_A);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->readTuple(ndbOpLockMode) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // set key attribute
        assert(*pa_id == ((i - 1) % nOps) + 1);
        if (op->equal(model->attr_id, (Int32)*pa_id) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // get attributes (not readable until after commit)
        if (op->getValue(model->attr_id, (char*)&pab->id) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_cint, (char*)&pab->cint) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_clong, (char*)&pab->clong) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_cfloat, (char*)&pab->cfloat) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_cdouble, (char*)&pab->cdouble) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());

        // execute the operation now if in non-batching mode
        if (!batch)
            executeOperations();
    }
    commitTransaction();
    closeTransaction();

    // release foreign key values holder
    delete[] a_id;

    // check fetched values
    pab = ab;
    for (int i = 1; i <= nOps; i++, pab++) {
        // check fetched values
        Int32 id = pab->id;
        VERIFY(id == ((i - 1) % nOps) + 1);

        Int32 j = getCommonAB(pab);
        //CDBG << "!!! id=" << toString(id) << ", i=" << toString(i) << endl;
        VERIFY(j == id);
    }

    // release attributes holder
    delete[] ab;
}

void
CrundNdbApiOperations::navAToB0(int nOps, bool forceSend)
{
    // attributes holder
    CommonAB h;

    // allocate attributes holder
    CommonAB* const ab = new CommonAB[nOps];

    // fetch attributes from B0 by foreign key scan
    beginTransaction();
    CommonAB* pab = ab;
    for (int i = 1; i <= nOps; i++) {
        // get an index scan operation for the table
        NdbIndexScanOperation* op
            = tx->getNdbIndexScanOperation(model->idx_B0_a_id);
        if (op == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());

        if (op->readTuples(ndbOpLockMode) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // define the scan's bounds (more efficient than using a scan filter)
        // the argument to setBound() is not the column's attribute id
        //    if (op->setBound(model->attr_B0_a_id, ...
        // or column name
        //    if (op->setBound("a_id", ...
        // but the attribute id of the column *in the index*.
        //    if (op->setBound(idx_B0_a_id->getColumn(0)->getAttrId()...
        // for which we introduced a shortcut.
        if (op->setBound(model->attr_idx_B0_a_id,
                         NdbIndexScanOperation::BoundEQ, &i) != 0)
            ABORT_NDB_ERROR(tx->getNdbError());

        // get attributes (not readable until after commit)
        if (op->getValue(model->attr_id, (char*)&h.id) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_cint, (char*)&h.cint) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_clong, (char*)&h.clong) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_cfloat, (char*)&h.cfloat) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());
        if (op->getValue(model->attr_cdouble, (char*)&h.cdouble) == NULL)
            ABORT_NDB_ERROR(tx->getNdbError());

        // start the scan; don't commit yet
        executeOperations();

        // read the result set executing the defined read operations
        int stat;
        const bool allowFetch = true; // request new batches when exhausted
        while ((stat = op->nextResult(allowFetch, forceSend)) == 0) {
            assert(ab <= pab && pab < ab + nOps);
            *pab++ = h;
        }
        if (stat != 1)
            ABORT_NDB_ERROR(tx->getNdbError());

        op->close();
    }
    commitTransaction();
    closeTransaction();
    //CDBG << "!!! pab - ab =" << toString(pab-ab) << endl;
    assert(pab == ab + nOps);

    // check fetched values
    // XXX this is not the most efficient way of testing...
    vector<CommonAB> b(ab, ab + nOps);
    sort(b.begin(), b.end(), compare);
    vector<CommonAB>::const_iterator it = b.begin();
    for (int i = 1; i <= nOps; i++, it++) {
        Int32 id = getCommonAB(&it[0]);
        //CDBG << "!!! id=" << toString(id) << ", i=" << toString(i) << endl;
        VERIFY(id == i);
    }

    // release attributes holder
    delete[] ab;
}

void
CrundNdbApiOperations::navAToB0alt(int nOps, bool forceSend)
{
    // number of operations in a multi-scan batch
    const int nmscans = (nOps < 256 ? nOps : 256);

    // attributes holder
    CommonAB h;

    // allocate attributes holder
    CommonAB* const ab = new CommonAB[nOps];
    CommonAB* pab = ab;

    // fetch attributes from B0 by foreign key scan
    beginTransaction();
    int a_id = 1;
    while (a_id <= nOps) {
        // allocate scan operations array
        NdbIndexScanOperation** const op = new NdbIndexScanOperation*[nmscans];

        for (int i = 0; i < nmscans; i++) {
            // get an index scan operation for the table
            op[i] = tx->getNdbIndexScanOperation(model->idx_B0_a_id);
            if (op[i] == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());

            // XXX ? no locks (LM_CommittedRead) or shared locks (LM_Read)
            if (op[i]->readTuples(ndbOpLockMode) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());

            // define the scan's bounds (more efficient than using a scan filter)
            // the argument to setBound() is not the column's attribute id
            //    if (op[i]->setBound(model->attr_B0_a_id, ...
            // or column name
            //    if (op[i]->setBound("a_id", ...
            // but the attribute id of the column *in the index*.
            //    if (op[i]->setBound(idx_B0_a_id->getColumn(0)->getAttrId()...
            // for which we introduced a shortcut.
            if (op[i]->setBound(model->attr_idx_B0_a_id,
                                NdbIndexScanOperation::BoundEQ, &a_id) != 0)
                ABORT_NDB_ERROR(tx->getNdbError());

            // get attributes (not readable until after commit)
            if (op[i]->getValue(model->attr_id, (char*)&h.id) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op[i]->getValue(model->attr_cint, (char*)&h.cint) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op[i]->getValue(model->attr_clong, (char*)&h.clong) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op[i]->getValue(model->attr_cfloat, (char*)&h.cfloat) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());
            if (op[i]->getValue(model->attr_cdouble, (char*)&h.cdouble) == NULL)
                ABORT_NDB_ERROR(tx->getNdbError());

            // next a
            a_id++;
        }
        executeOperations(); // start the scans; don't commit yet

        // fetch attributes from B0 by foreign key scan
        for (int i = 0; i < nmscans; i++) {
            // read the result set executing the defined read operations
            int stat;
            const bool allowFetch = true; // request new batches when exhausted
            while ((stat = op[i]->nextResult(allowFetch, forceSend)) == 0) {
                assert(ab <= pab && pab < ab + nOps);
                *pab++ = h;
            }
            if (stat != 1)
                ABORT_NDB_ERROR(tx->getNdbError());

            op[i]->close();
        }

        // release scan operations array
        delete[] op;
    }
    commitTransaction();
    closeTransaction();
    //CDBG << "!!! pab - ab =" << toString(pab-ab) << endl;
    assert(a_id == nOps + 1);
    assert(pab == ab + nOps);

    // check fetched values
    // XXX this is not the most efficient way of testing...
    vector<CommonAB> b(ab, ab + nOps);
    sort(b.begin(), b.end(), compare);
    vector<CommonAB>::const_iterator it = b.begin();
    for (int i = 1; i <= nOps; i++, it++) {
        Int32 id = getCommonAB(&it[0]);
        //CDBG << "!!! id=" << toString(id) << ", i=" << toString(i) << endl;
        VERIFY(id == i);
    }

    // release attributes holder
    delete[] ab;
}

// ----------------------------------------------------------------------

