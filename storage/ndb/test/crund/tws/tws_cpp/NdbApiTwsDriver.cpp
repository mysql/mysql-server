/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

#include "NdbApiTwsDriver.hpp"

#include <iostream>
#include <string>
#include <cassert>

#include "helpers.hpp"
#include "string_helpers.hpp"
#include "Properties.hpp"

using std::cout;
using std::flush;
using std::endl;
using std::ios_base;
using std::string;

using utils::Properties;
using utils::toBool;
using utils::toInt;
using utils::toString;

// ---------------------------------------------------------------------------
// Helper Macros & Functions
// ---------------------------------------------------------------------------

// This benchmark's error handling of NDBAPI calls is rigorous but crude:
// - all calls' return value is checked for errors
// - all errors are reported and then followed by a process exit
#define ABORT_NDB_ERROR(error)                                          \
    do { cout << "!!! error in " << __FILE__ << ", line: " << __LINE__  \
              << ", code: " << (int)(error).code                        \
              << ", msg: " << (error).message << "." << endl;           \
        exit(-1);                                                       \
    } while (0)

// ---------------------------------------------------------------------------
// NDB API Model Implementation
// ---------------------------------------------------------------------------

NdbApiTwsModel::NdbApiTwsModel(Ndb* ndb) {
    const NdbDictionary::Dictionary* dict = ndb->getDictionary();

    // get table T0
    if ((table_t0 = dict->getTable("mytable")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());

    // get columns of table T0
    if ((column_c0 = table_t0->getColumn("c0")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c1 = table_t0->getColumn("c1")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c2 = table_t0->getColumn("c2")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c3 = table_t0->getColumn("c3")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c4 = table_t0->getColumn("c4")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c5 = table_t0->getColumn("c5")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c6 = table_t0->getColumn("c6")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c7 = table_t0->getColumn("c7")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c8 = table_t0->getColumn("c8")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c9 = table_t0->getColumn("c9")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c10 = table_t0->getColumn("c10")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c11 = table_t0->getColumn("c11")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c12 = table_t0->getColumn("c12")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c13 = table_t0->getColumn("c13")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());
    if ((column_c14 = table_t0->getColumn("c14")) == NULL)
        ABORT_NDB_ERROR(dict->getNdbError());

    // get attribute ids for columns of table T0
    attr_c0 = column_c0->getAttrId();
    attr_c1 = column_c1->getAttrId();
    attr_c2 = column_c2->getAttrId();
    attr_c3 = column_c3->getAttrId();
    attr_c4 = column_c4->getAttrId();
    attr_c5 = column_c5->getAttrId();
    attr_c6 = column_c6->getAttrId();
    attr_c7 = column_c7->getAttrId();
    attr_c8 = column_c8->getAttrId();
    attr_c9 = column_c9->getAttrId();
    attr_c10 = column_c10->getAttrId();
    attr_c11 = column_c11->getAttrId();
    attr_c12 = column_c12->getAttrId();
    attr_c13 = column_c13->getAttrId();
    attr_c14 = column_c14->getAttrId();

    width_c0 = columnWidth(column_c0);
    width_c1 = columnWidth(column_c1);
    width_c2 = columnWidth(column_c2);
    width_c3 = columnWidth(column_c3);
    width_c4 = columnWidth(column_c4);
    width_c5 = columnWidth(column_c5);
    width_c6 = columnWidth(column_c6);
    width_c7 = columnWidth(column_c7);
    width_c8 = columnWidth(column_c8);
    width_c9 = columnWidth(column_c9);
    width_c10 = columnWidth(column_c10);
    width_c11 = columnWidth(column_c11);
    width_c12 = columnWidth(column_c12);
    width_c13 = columnWidth(column_c13);
    width_c14 = columnWidth(column_c14);

    width_row = (
        + width_c0
        + width_c1
        + width_c2
        + width_c3
        + width_c4
        + width_c5
        + width_c6
        + width_c7
        + width_c8
        + width_c9
        + width_c10
        + width_c11
        + width_c12
        + width_c13
        + width_c14);
}

// ---------------------------------------------------------------------------
// NdbApiTwsDriver Implementation
// ---------------------------------------------------------------------------

void
NdbApiTwsDriver::init() {
    TwsDriver::init();

    // ndb_init must be called first
    cout << endl
         << "initializing NDBAPI ..." << flush;
    int stat = ndb_init();
    if (stat != 0)
        ABORT_ERROR("ndb_init() returned: " << stat);
    cout << "         [ok]" << endl;

    initConnection();
}

void
NdbApiTwsDriver::close() {
    closeConnection();

    // ndb_close must be called last
    cout << "closing NDBAPI ...   " << flush;
    ndb_end(0);
    cout << "           [ok]" << endl;

    TwsDriver::close();
}

void
NdbApiTwsDriver::initProperties() {
    TwsDriver::initProperties();

    cout << "setting ndb properties ..." << flush;

    ostringstream msg;

    mgmdConnect = toString(props[L"ndb.mgmdConnect"]);
    if (mgmdConnect.empty()) {
        mgmdConnect = string("localhost");
    }

    catalog = toString(props[L"ndb.catalog"]);
    if (catalog.empty()) {
        catalog = string("testdb");
    }

    schema = toString(props[L"ndb.schema"]);
    if (schema.empty()) {
        schema = string("def");
    }

    //if (msg.tellp() == 0) {
    if (msg.str().empty()) {
        cout << "      [ok]" << endl;
    } else {
        cout << endl << msg.str() << endl;
    }

    descr = "ndbapi(" + mgmdConnect + ")";
}

void
NdbApiTwsDriver::printProperties() {
    TwsDriver::printProperties();

    const ios_base::fmtflags f = cout.flags();
    // no effect calling manipulator function, not sure why
    //cout << ios_base::boolalpha;
    cout.flags(ios_base::boolalpha);

    cout << endl << "ndb settings ..." << endl;
    cout << "ndb.mgmdConnect:                \"" << mgmdConnect << "\"" << endl;
    cout << "ndb.catalog:                    \"" << catalog << "\"" << endl;
    cout << "ndb.schema:                     \"" << schema << "\"" << endl;

    cout.flags(f);
}

void
NdbApiTwsDriver::initNdbapiBuffers() {
    assert(model->column_c0 != NULL);
    assert(bb == NULL);
    assert(ra == NULL);

    cout << "allocating ndbapi buffers ..." << flush;
    bb = new char[model->width_row * nRows];
    ra = new NdbRecAttr*[model->nCols * nRows];
    cout << "   [ok]" << endl;
}

void
NdbApiTwsDriver::closeNdbapiBuffers() {
    assert(bb != NULL);
    assert(ra != NULL);

    cout << "releasing ndbapi buffers ..." << flush;
    delete[] ra;
    ra = NULL;
    delete[] bb;
    bb = NULL;
    cout << "    [ok]" << endl;
}

// ---------------------------------------------------------------------------

void
NdbApiTwsDriver::runLoadOperations() {
    cout << endl
         << "running NDB API operations ..." << "  [nRows=" << nRows << "]"
         << endl;

    if (doSingle) {
        if (doInsert) runNdbapiInsert(SINGLE);
        if (doLookup) runNdbapiLookup(SINGLE);
        if (doUpdate) runNdbapiUpdate(SINGLE);
        if (doDelete) runNdbapiDelete(SINGLE);
    }
    if (doBulk) {
        if (doInsert) runNdbapiInsert(BULK);
        if (doLookup) runNdbapiLookup(BULK);
        if (doUpdate) runNdbapiUpdate(BULK);
        if (doDelete) runNdbapiDelete(BULK);
    }
    if (doBatch) {
        if (doInsert) runNdbapiInsert(BATCH);
        if (doLookup) runNdbapiLookup(BATCH);
        if (doUpdate) runNdbapiUpdate(BATCH);
        if (doDelete) runNdbapiDelete(BATCH);
    }
}

// Alternative Implementation:
// The recurring pattern of runNdbapiXXX(XMode mode) can be parametrized
// over the operation, for instance, by a member function template:
//
//    template< XMode M, void (NdbApiTwsDriver::*OP)(int) >
//    void runOp(string name);
//
// which tests the template parameter XMode and calls the operation
//    ...
//    if (M == SINGLE) {
//       ... (this->*OP)(i); ...
//    }
//    ...
// while the caller selects the mode and function to be invoked
//    ...
//    switch (mode) {
//    case SINGLE:
//        runOp< SINGLE, &NdbApiTwsDriver::ndbapiInsert >(name);
//    }...
// Alas, it turns out not worth it in terms of readability and lines of code.

void
NdbApiTwsDriver::runNdbapiInsert(XMode mode) {
    const string name = string("insert_") + toStr(mode);
    begin(name);

    if (mode == SINGLE) {
            for(int i = 0; i < nRows; i++) {
            ndbapiBeginTransaction();
            ndbapiInsert(i);
            ndbapiCommitTransaction();
            ndbapiCloseTransaction();
        }
    } else {
        ndbapiBeginTransaction();
        for(int i = 0; i < nRows; i++) {
            ndbapiInsert(i);
            if (mode == BULK)
                ndbapiExecuteTransaction();
        }
        ndbapiCommitTransaction();
        ndbapiCloseTransaction();
    }

    finish(name);
}

void
NdbApiTwsDriver::ndbapiInsert(int c0) {
    // get an insert operation for the table
    NdbOperation* op = tx->getNdbOperation(model->table_t0);
    if (op == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    if (op->insertTuple() != 0)
        ABORT_NDB_ERROR(tx->getNdbError());

    // values
    Uint32 i = c0;
    const int maxlen = 256;
    char str[maxlen];
    snprintf(str, maxlen, "%d", i);

    // set values; key attribute needs to be set first
    ndbapiToBuffer1blp(bb_pos, str, model->width_c0);
    if (op->equal(model->attr_c0, bb_pos) != 0) // key
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c0;

    ndbapiToBuffer1blp(bb_pos, str, model->width_c1);
    if (op->setValue(model->attr_c1, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c1;

    if (op->setValue(model->attr_c2, i) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c2;

    if (op->setValue(model->attr_c3, i) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c3;

    if (op->setValue(model->attr_c4, (char*)NULL) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c4;

    ndbapiToBuffer1blp(bb_pos, str, model->width_c5);
    if (op->setValue(model->attr_c5, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c5;

    ndbapiToBuffer1blp(bb_pos, str, model->width_c6);
    if (op->setValue(model->attr_c6, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c6;

    ndbapiToBuffer1blp(bb_pos, str, model->width_c7);
    if (op->setValue(model->attr_c7, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c7;

    ndbapiToBuffer1blp(bb_pos, str, model->width_c8);
    if (op->setValue(model->attr_c8, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c8;

    if (op->setValue(model->attr_c9, (char*)NULL) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c9;

    if (op->setValue(model->attr_c10, (char*)NULL) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c10;

    if (op->setValue(model->attr_c11, (char*)NULL) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c11;

    if (op->setValue(model->attr_c12, (char*)NULL) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c12;

    if (op->setValue(model->attr_c13, (char*)NULL) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c13;

    if (op->setValue(model->attr_c14, (char*)NULL) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c14;
}

void
NdbApiTwsDriver::runNdbapiLookup(XMode mode) {
    const string name = string("lookup_") + toStr(mode);
    begin(name);

    if (mode == SINGLE) {
        for(int i = 0; i < nRows; i++) {
            ndbapiBeginTransaction();
            ndbapiLookup(i);
            ndbapiCommitTransaction();
            ndbapiRead(i);
            ndbapiCloseTransaction();
        }
    } else {
        ndbapiBeginTransaction();
        for(int i = 0; i < nRows; i++) {
            ndbapiLookup(i);

            if (mode == BULK)
                ndbapiExecuteTransaction();
        }
        ndbapiCommitTransaction();
        for(int i = 0; i < nRows; i++) {
            ndbapiRead(i);
        }
        ndbapiCloseTransaction();
    }

    finish(name);
}

void
NdbApiTwsDriver::ndbapiLookup(int c0) {
    // get a lookup operation for the table
    NdbOperation* op = tx->getNdbOperation(model->table_t0);
    if (op == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    if (op->readTuple(ndbOpLockMode) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());

    // values
    Uint32 i = c0;
    const int maxlen = 256;
    char str[maxlen];
    snprintf(str, maxlen, "%d", i);

    // set values; key attribute needs to be set first
    ndbapiToBuffer1blp(bb_pos, str, model->width_c0);
    if (op->equal(model->attr_c0, bb_pos) != 0) // key
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c0;

    // get attributes (not readable until after commit)
    if ((*ra_pos = op->getValue(model->attr_c1, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c1;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c2, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c2;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c3, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c3;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c4, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c4;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c5, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c5;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c6, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c6;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c7, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c7;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c8, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c8;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c9, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c9;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c10, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c10;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c11, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c11;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c12, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c12;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c13, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c13;
    ra_pos++;

    if ((*ra_pos = op->getValue(model->attr_c14, bb_pos)) == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c14;
    ra_pos++;
}

void
NdbApiTwsDriver::ndbapiRead(int c0) {
    // values
    const int maxlen = 256;
    char str0[maxlen];
    snprintf(str0, maxlen, "%d", c0);
    Int32 i1;
    char str1[maxlen];

    // no need to read key column
    bb_pos += model->width_c0;

    ndbapiToString1blp(str1, bb_pos, model->width_c1);
    verify(str0, str1);
    bb_pos += model->width_c1;
    ra_pos++;

    memcpy(&i1, bb_pos, model->width_c2);
    verify(c0, i1);
    bb_pos += model->width_c2;
    ra_pos++;

    memcpy(&i1, bb_pos, model->width_c3);
    verify(c0, i1);
    bb_pos += model->width_c3;
    ra_pos++;

    // null expected
    verify(1, (*ra_pos)->isNULL());
    bb_pos += model->width_c4;
    ra_pos++;

    ndbapiToString1blp(str1, bb_pos, model->width_c5);
    verify(str0, str1);
    bb_pos += model->width_c5;
    ra_pos++;

    ndbapiToString1blp(str1, bb_pos, model->width_c6);
    verify(str0, str1);
    bb_pos += model->width_c6;
    ra_pos++;

    ndbapiToString1blp(str1, bb_pos, model->width_c7);
    verify(str0, str1);
    bb_pos += model->width_c7;
    ra_pos++;

    ndbapiToString1blp(str1, bb_pos, model->width_c8);
    verify(str0, str1);
    bb_pos += model->width_c8;
    ra_pos++;

    // null expected
    verify(1, (*ra_pos)->isNULL());
    bb_pos += model->width_c9;
    ra_pos++;

    // null expected
    verify(1, (*ra_pos)->isNULL());
    bb_pos += model->width_c10;
    ra_pos++;

    // null expected
    verify(1, (*ra_pos)->isNULL());
    bb_pos += model->width_c11;
    ra_pos++;

    // null expected
    verify(1, (*ra_pos)->isNULL());
    bb_pos += model->width_c12;
    ra_pos++;

    // null expected
    verify(1, (*ra_pos)->isNULL());
    bb_pos += model->width_c13;
    ra_pos++;

    // null expected
    verify(1, (*ra_pos)->isNULL());
    bb_pos += model->width_c14;
    ra_pos++;
}

void
NdbApiTwsDriver::runNdbapiUpdate(XMode mode) {
    const string name = string("update_") + toStr(mode);
    begin(name);

    if (mode == SINGLE) {
            for(int i = 0; i < nRows; i++) {
            ndbapiBeginTransaction();
            ndbapiUpdate(i);
            ndbapiCommitTransaction();
            ndbapiCloseTransaction();
        }
    } else {
        ndbapiBeginTransaction();
        for(int i = 0; i < nRows; i++) {
            ndbapiUpdate(i);
            if (mode == BULK)
                ndbapiExecuteTransaction();
        }
        ndbapiCommitTransaction();
        ndbapiCloseTransaction();
    }

    finish(name);
}

void
NdbApiTwsDriver::ndbapiUpdate(int c0) {
    // get an update operation for the table
    NdbOperation* op = tx->getNdbOperation(model->table_t0);
    if (op == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    if (op->updateTuple() != 0)
        ABORT_NDB_ERROR(tx->getNdbError());

    // values
    const int maxlen = 256;
    char str0[maxlen];
    snprintf(str0, maxlen, "%d", c0);
    int i = -c0;
    char str1[maxlen];
    snprintf(str1, maxlen, "%d", i);

    // set values; key attribute needs to be set first
    ndbapiToBuffer1blp(bb_pos, str0, model->width_c0);
    if (op->equal(model->attr_c0, bb_pos) != 0) // key
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c0;

    ndbapiToBuffer1blp(bb_pos, str1, model->width_c1);
    if (op->setValue(model->attr_c1, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c1;

    if (op->setValue(model->attr_c2, i) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c2;

    if (op->setValue(model->attr_c3, i) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c3;

    ndbapiToBuffer1blp(bb_pos, str1, model->width_c5);
    if (op->setValue(model->attr_c5, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c5;

    ndbapiToBuffer1blp(bb_pos, str1, model->width_c6);
    if (op->setValue(model->attr_c6, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c6;

    ndbapiToBuffer1blp(bb_pos, str1, model->width_c7);
    if (op->setValue(model->attr_c7, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c7;

    ndbapiToBuffer1blp(bb_pos, str1, model->width_c8);
    if (op->setValue(model->attr_c8, bb_pos) != 0)
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c8;
}

void
NdbApiTwsDriver::runNdbapiDelete(XMode mode) {
    const string name = string("delete_") + toStr(mode);
    begin(name);

    if (mode == SINGLE) {
            for(int i = 0; i < nRows; i++) {
            ndbapiBeginTransaction();
            ndbapiDelete(i);
            ndbapiCommitTransaction();
            ndbapiCloseTransaction();
        }
    } else {
        ndbapiBeginTransaction();
        for(int i = 0; i < nRows; i++) {
            ndbapiDelete(i);
            if (mode == BULK)
                ndbapiExecuteTransaction();
        }
        ndbapiCommitTransaction();
        ndbapiCloseTransaction();
    }

    finish(name);
}

void
NdbApiTwsDriver::ndbapiDelete(int c0) {
    // get a delete operation for the table
    NdbOperation* op = tx->getNdbOperation(model->table_t0);
    if (op == NULL)
        ABORT_NDB_ERROR(tx->getNdbError());
    if (op->deleteTuple() != 0)
        ABORT_NDB_ERROR(tx->getNdbError());

    // values
    Uint32 i = c0;
    const int maxlen = 256;
    char str[maxlen];
    snprintf(str, maxlen, "%d", i);

    // set values; key attribute needs to be set first
    ndbapiToBuffer1blp(bb_pos, str, model->width_c0);
    if (op->equal(model->attr_c0, bb_pos) != 0) // key
        ABORT_NDB_ERROR(tx->getNdbError());
    bb_pos += model->width_c0;
}

// ----------------------------------------------------------------------

void
NdbApiTwsDriver::ndbapiBeginTransaction() {
    assert(tx == NULL);

    // prepare buffer for writing
    bb_pos = bb; // clear
    ra_pos = ra;

    // start a transaction
    // must be closed with Ndb::closeTransaction or NdbTransaction::close
    if ((tx = ndb->startTransaction()) == NULL)
        ABORT_NDB_ERROR(ndb->getNdbError());
}

void
NdbApiTwsDriver::ndbapiExecuteTransaction() {
    assert(tx != NULL);

    // execute but don't commit the current transaction
    if (tx->execute(NdbTransaction::NoCommit) != 0
        || tx->getNdbError().status != NdbError::Success)
        ABORT_NDB_ERROR(tx->getNdbError());
}

void
NdbApiTwsDriver::ndbapiCommitTransaction() {
    assert(tx != NULL);

    // commit the current transaction
    if (tx->execute(NdbTransaction::Commit) != 0
        || tx->getNdbError().status != NdbError::Success)
        ABORT_NDB_ERROR(tx->getNdbError());

    // prepare buffer for reading
    bb_pos = bb; // rewind
    ra_pos = ra;
}

void
NdbApiTwsDriver::ndbapiCloseTransaction() {
    assert(tx != NULL);

    // close the current transaction
    // to be called irrespectively of success or failure
    ndb->closeTransaction(tx);
    tx = NULL;
}

// ---------------------------------------------------------------------------

void
NdbApiTwsDriver::initConnection() {
    assert(mgmd == NULL);
    assert(ndb == NULL);
    assert(tx == NULL);
    assert(model == NULL);

    cout << endl;

    // instantiate NDB cluster singleton
    cout << "creating cluster connection ..." << flush;
    assert(!mgmdConnect.empty());
    mgmd = new Ndb_cluster_connection(mgmdConnect.c_str());
    cout << " [ok]" << endl; // no useful mgmd->string conversion

    // connect to cluster management node (ndb_mgmd)
    cout << "connecting to mgmd ..." << flush;
    const int retries = 0; // number of retries (< 0 = indefinitely)
    const int delay = 0;   // seconds to wait after retry
    const int verbose = 1; // print report of progess
    // returns: 0 = success, 1 = recoverable error, -1 = non-recoverable error
    if (mgmd->connect(retries, delay, verbose) != 0)
        ABORT_ERROR("mgmd@" << mgmdConnect << " was not ready within "
                     << (retries * delay) << "s.");
    cout << "          [ok: " << mgmdConnect << "]" << endl;

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
    ndb = new Ndb(mgmd, catalog.c_str(), schema.c_str());
    const int max_no_tx = 10; // maximum number of parallel tx (<=1024)
    // note each scan or index scan operation uses one extra transaction
    //if (ndb->init() != 0)
    if (ndb->init(max_no_tx) != 0)
        ABORT_NDB_ERROR(ndb->getNdbError());
    cout << "      [ok: " << catalog << "." << schema << "]" << endl;

    cout << "caching metadata ..." << flush;
    model = new NdbApiTwsModel(ndb);
    cout << "            [ok]" << endl;

    initNdbapiBuffers();

    cout << "using lock mode for reads ..." << flush;
    string lm;
    switch (lockMode) {
    case READ_COMMITTED:
        ndbOpLockMode = NdbOperation::LM_CommittedRead;
        lm = "LM_CommittedRead";
        break;
    case SHARED:
        ndbOpLockMode = NdbOperation::LM_Read;
        lm = "LM_Read";
        break;
    case EXCLUSIVE:
        ndbOpLockMode = NdbOperation::LM_Exclusive;
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
NdbApiTwsDriver::closeConnection() {
    assert(mgmd != NULL);
    assert(ndb != NULL);
    assert(tx == NULL);
    assert(model != NULL);

    cout << endl;

    closeNdbapiBuffers();

    cout << "clearing metadata cache ..." << flush;
    delete model;
    model = NULL;
    cout << "     [ok]" << endl;

    cout << "closing database connection ..." << flush;
    // no ndb->close();
    delete ndb;
    ndb = NULL;
    cout << " [ok]" << endl;

    cout << "closing cluster connection ..." << flush;
    delete mgmd;
    mgmd = NULL;
    cout << "  [ok]" << endl;
}

//---------------------------------------------------------------------------

void
NdbApiTwsDriver::ndbapiToBuffer1blp(void* to, const char* from, size_t width) {
    char* t = (char*)to;
    size_t n = strlen(from);
    assert(0 <= n && n < width && width < 256); // width <= 256 ?

    memcpy(t + 1, from, n);
    *t = n;
}

void
NdbApiTwsDriver::ndbapiToString1blp(char* to, const void* from, size_t width) {
    const char* s = (const char*)from;
    size_t n = *s;
    assert(0 <= n && n < width && width < 256); // width <= 256 ?

    memcpy(to, s + 1, n);
    to[n] = '\0';
}

// ---------------------------------------------------------------------------

int
main(int argc, const char* argv[])
{
    NdbApiTwsDriver::parseArguments(argc, argv);
    NdbApiTwsDriver d;
    d.run();

    return 0;
}

//---------------------------------------------------------------------------
