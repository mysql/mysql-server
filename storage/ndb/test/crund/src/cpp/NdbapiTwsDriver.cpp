/*
  Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

#include "NdbapiTwsDriver.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>

#include "helpers.hpp"
#include "string_helpers.hpp"

using std::cout;
using std::endl;
using std::flush;
using std::ostringstream;
using std::string;

// ---------------------------------------------------------------------------
// Helper Macros & Functions
// ---------------------------------------------------------------------------

// Current error handling is crude & simple:
// - all calls' return value is checked for errors
// - all errors are reported and then followed by a process exit

#define ABORT_NDB_ERROR(error)                                              \
  do {                                                                      \
    cout << "!!! error in " << __FILE__ << ", line: " << __LINE__           \
         << ", code: " << (int)(error).code << ", msg: " << (error).message \
         << "." << endl;                                                    \
    exit(-1);                                                               \
  } while (0)

// ---------------------------------------------------------------------------
// NDB API Model Implementation
// ---------------------------------------------------------------------------

NdbapiTwsModel::NdbapiTwsModel(Ndb *ndb) {
  const NdbDictionary::Dictionary *dict = ndb->getDictionary();

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

  width_row = (+width_c0 + width_c1 + width_c2 + width_c3 + width_c4 +
               width_c5 + width_c6 + width_c7 + width_c8 + width_c9 +
               width_c10 + width_c11 + width_c12 + width_c13 + width_c14);
}

// ---------------------------------------------------------------------------
// NdbapiTwsDriver Implementation
// ---------------------------------------------------------------------------

void NdbapiTwsDriver::init() {
  TwsDriver::init();

  // ndb_init must be called first
  cout << endl << "initializing NDBAPI ..." << flush;
  int stat = ndb_init();
  if (stat != 0) ABORT_ERROR("ndb_init() returned: " << stat);
  cout << "         [ok]" << endl;
}

void NdbapiTwsDriver::close() {
  // ndb_close must be called last
  cout << "closing NDBAPI ...   " << flush;
  ndb_end(0);
  cout << "           [ok]" << endl;

  TwsDriver::close();
}

void NdbapiTwsDriver::initProperties() {
  TwsDriver::initProperties();

  cout << "setting ndb properties ..." << flush;

  ostringstream msg;

  mgmdConnect = toS(props[L"ndb.mgmdConnect"], L"localhost");
  catalog = toS(props[L"ndb.catalog"], L"crunddb");
  schema = toS(props[L"ndb.schema"], L"def");

  // if (msg.tellp() == 0) {
  if (msg.str().empty()) {
    cout << "      [ok]" << endl;
  } else {
    cout << endl << msg.str() << endl;
  }

  descr = "ndbapi(" + mgmdConnect + ")";
}

void NdbapiTwsDriver::printProperties() {
  TwsDriver::printProperties();

  cout << endl << "ndb settings ..." << endl;
  cout << "ndb.mgmdConnect:                \"" << mgmdConnect << "\"" << endl;
  cout << "ndb.catalog:                    \"" << catalog << "\"" << endl;
  cout << "ndb.schema:                     \"" << schema << "\"" << endl;
}

// ---------------------------------------------------------------------------

void NdbapiTwsDriver::initOperations() {}

void NdbapiTwsDriver::closeOperations() {}

// XXX call buffer init/close from elsewhere
void NdbapiTwsDriver::runOperations(int nOps) {
  initNdbapiBuffers(nOps);
  TwsDriver::runOperations(nOps);
  closeNdbapiBuffers(nOps);
}

void NdbapiTwsDriver::runInserts(XMode mode, int nOps) {
  const string name = string("insert_") + toStr(mode);
  beginOp(name);

  if (mode == INDY) {
    for (int i = 0; i < nOps; i++) {
      ndbapiBeginTransaction();
      ndbapiInsert(i);
      ndbapiCommitTransaction();
      ndbapiCloseTransaction();
    }
  } else {
    ndbapiBeginTransaction();
    for (int i = 0; i < nOps; i++) {
      ndbapiInsert(i);
      if (mode == EACH) ndbapiExecuteTransaction();
    }
    ndbapiCommitTransaction();
    ndbapiCloseTransaction();
  }

  finishOp(name, nOps);
}

void NdbapiTwsDriver::runLookups(XMode mode, int nOps) {
  const string name = string("lookup_") + toStr(mode);
  beginOp(name);

  if (mode == INDY) {
    for (int i = 0; i < nOps; i++) {
      ndbapiBeginTransaction();
      ndbapiLookup(i);
      ndbapiCommitTransaction();
      ndbapiRead(i);
      ndbapiCloseTransaction();
    }
  } else {
    ndbapiBeginTransaction();
    for (int i = 0; i < nOps; i++) {
      ndbapiLookup(i);

      if (mode == EACH) ndbapiExecuteTransaction();
    }
    ndbapiCommitTransaction();
    for (int i = 0; i < nOps; i++) {
      ndbapiRead(i);
    }
    ndbapiCloseTransaction();
  }

  finishOp(name, nOps);
}

void NdbapiTwsDriver::runUpdates(XMode mode, int nOps) {
  const string name = string("update_") + toStr(mode);
  beginOp(name);

  if (mode == INDY) {
    for (int i = 0; i < nOps; i++) {
      ndbapiBeginTransaction();
      ndbapiUpdate(i);
      ndbapiCommitTransaction();
      ndbapiCloseTransaction();
    }
  } else {
    ndbapiBeginTransaction();
    for (int i = 0; i < nOps; i++) {
      ndbapiUpdate(i);
      if (mode == EACH) ndbapiExecuteTransaction();
    }
    ndbapiCommitTransaction();
    ndbapiCloseTransaction();
  }

  finishOp(name, nOps);
}

void NdbapiTwsDriver::runDeletes(XMode mode, int nOps) {
  const string name = string("delete_") + toStr(mode);
  beginOp(name);

  if (mode == INDY) {
    for (int i = 0; i < nOps; i++) {
      ndbapiBeginTransaction();
      ndbapiDelete(i);
      ndbapiCommitTransaction();
      ndbapiCloseTransaction();
    }
  } else {
    ndbapiBeginTransaction();
    for (int i = 0; i < nOps; i++) {
      ndbapiDelete(i);
      if (mode == EACH) ndbapiExecuteTransaction();
    }
    ndbapiCommitTransaction();
    ndbapiCloseTransaction();
  }

  finishOp(name, nOps);
}

void NdbapiTwsDriver::initNdbapiBuffers(int nOps) {
  assert(model->column_c0 != NULL);
  assert(bb == NULL);
  assert(ra == NULL);

  cout << "allocating ndbapi buffers ..." << flush;
  bb = new char[model->width_row * nOps];
  ra = new NdbRecAttr *[model->nCols * nOps];
  cout << "   [ok]" << endl;
}

void NdbapiTwsDriver::closeNdbapiBuffers(int nOps) {
  assert(bb != NULL);
  assert(ra != NULL);

  cout << "releasing ndbapi buffers ..." << flush;
  delete[] ra;
  ra = NULL;
  delete[] bb;
  bb = NULL;
  cout << "    [ok]" << endl;
}

void NdbapiTwsDriver::ndbapiInsert(int c0) {
  // get an insert operation for the table
  NdbOperation *op = tx->getNdbOperation(model->table_t0);
  if (op == NULL) ABORT_NDB_ERROR(tx->getNdbError());
  if (op->insertTuple() != 0) ABORT_NDB_ERROR(tx->getNdbError());

  // values
  Uint32 i = c0;
  const int maxlen = 256;
  char str[maxlen];
  snprintf(str, maxlen, "%d", i);

  // set values; key attribute needs to be set first
  ndbapiToBuffer1blp(bb_pos, str, model->width_c0);
  if (op->equal(model->attr_c0, bb_pos) != 0)  // key
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c0;

  ndbapiToBuffer1blp(bb_pos, str, model->width_c1);
  if (op->setValue(model->attr_c1, bb_pos) != 0)
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c1;

  if (op->setValue(model->attr_c2, i) != 0) ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c2;

  if (op->setValue(model->attr_c3, i) != 0) ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c3;

  if (op->setValue(model->attr_c4, (char *)NULL) != 0)
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

  if (op->setValue(model->attr_c9, (char *)NULL) != 0)
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c9;

  if (op->setValue(model->attr_c10, (char *)NULL) != 0)
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c10;

  if (op->setValue(model->attr_c11, (char *)NULL) != 0)
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c11;

  if (op->setValue(model->attr_c12, (char *)NULL) != 0)
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c12;

  if (op->setValue(model->attr_c13, (char *)NULL) != 0)
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c13;

  if (op->setValue(model->attr_c14, (char *)NULL) != 0)
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c14;
}

void NdbapiTwsDriver::ndbapiLookup(int c0) {
  // get a lookup operation for the table
  NdbOperation *op = tx->getNdbOperation(model->table_t0);
  if (op == NULL) ABORT_NDB_ERROR(tx->getNdbError());
  if (op->readTuple(ndbOpLockMode) != 0) ABORT_NDB_ERROR(tx->getNdbError());

  // values
  Uint32 i = c0;
  const int maxlen = 256;
  char str[maxlen];
  snprintf(str, maxlen, "%d", i);

  // set values; key attribute needs to be set first
  ndbapiToBuffer1blp(bb_pos, str, model->width_c0);
  if (op->equal(model->attr_c0, bb_pos) != 0)  // key
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

void NdbapiTwsDriver::ndbapiRead(int c0) {
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

void NdbapiTwsDriver::ndbapiUpdate(int c0) {
  // get an update operation for the table
  NdbOperation *op = tx->getNdbOperation(model->table_t0);
  if (op == NULL) ABORT_NDB_ERROR(tx->getNdbError());
  if (op->updateTuple() != 0) ABORT_NDB_ERROR(tx->getNdbError());

  // values
  const int maxlen = 256;
  char str0[maxlen];
  snprintf(str0, maxlen, "%d", c0);
  int i = -c0;
  char str1[maxlen];
  snprintf(str1, maxlen, "%d", i);

  // set values; key attribute needs to be set first
  ndbapiToBuffer1blp(bb_pos, str0, model->width_c0);
  if (op->equal(model->attr_c0, bb_pos) != 0)  // key
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c0;

  ndbapiToBuffer1blp(bb_pos, str1, model->width_c1);
  if (op->setValue(model->attr_c1, bb_pos) != 0)
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c1;

  if (op->setValue(model->attr_c2, i) != 0) ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c2;

  if (op->setValue(model->attr_c3, i) != 0) ABORT_NDB_ERROR(tx->getNdbError());
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

void NdbapiTwsDriver::ndbapiDelete(int c0) {
  // get a delete operation for the table
  NdbOperation *op = tx->getNdbOperation(model->table_t0);
  if (op == NULL) ABORT_NDB_ERROR(tx->getNdbError());
  if (op->deleteTuple() != 0) ABORT_NDB_ERROR(tx->getNdbError());

  // values
  Uint32 i = c0;
  const int maxlen = 256;
  char str[maxlen];
  snprintf(str, maxlen, "%d", i);

  // set values; key attribute needs to be set first
  ndbapiToBuffer1blp(bb_pos, str, model->width_c0);
  if (op->equal(model->attr_c0, bb_pos) != 0)  // key
    ABORT_NDB_ERROR(tx->getNdbError());
  bb_pos += model->width_c0;
}

// ----------------------------------------------------------------------

void NdbapiTwsDriver::ndbapiBeginTransaction() {
  assert(tx == NULL);

  // prepare buffer for writing
  bb_pos = bb;  // clear
  ra_pos = ra;

  // start a transaction
  // must be closed with Ndb::closeTransaction or NdbTransaction::close
  if ((tx = ndb->startTransaction()) == NULL)
    ABORT_NDB_ERROR(ndb->getNdbError());
}

void NdbapiTwsDriver::ndbapiExecuteTransaction() {
  assert(tx != NULL);

  // execute but don't commit the current transaction
  if (tx->execute(NdbTransaction::NoCommit) != 0 ||
      tx->getNdbError().status != NdbError::Success)
    ABORT_NDB_ERROR(tx->getNdbError());
}

void NdbapiTwsDriver::ndbapiCommitTransaction() {
  assert(tx != NULL);

  // commit the current transaction
  if (tx->execute(NdbTransaction::Commit) != 0 ||
      tx->getNdbError().status != NdbError::Success)
    ABORT_NDB_ERROR(tx->getNdbError());

  // prepare buffer for reading
  bb_pos = bb;  // rewind
  ra_pos = ra;
}

void NdbapiTwsDriver::ndbapiCloseTransaction() {
  assert(tx != NULL);

  // close the current transaction
  // to be called irrespectively of success or failure
  ndb->closeTransaction(tx);
  tx = NULL;
}

// ---------------------------------------------------------------------------

void NdbapiTwsDriver::initConnection() {
  assert(mgmd == NULL);
  assert(ndb == NULL);
  assert(tx == NULL);
  assert(model == NULL);

  cout << endl;

  // instantiate NDB cluster singleton
  cout << "creating cluster connection ..." << flush;
  assert(!mgmdConnect.empty());
  mgmd = new Ndb_cluster_connection(mgmdConnect.c_str());
  mgmd->configure_tls(opt_tls_search_path, opt_mgm_tls);
  cout << " [ok: mgmd@" << mgmdConnect << "]" << endl;

  // connect to cluster management node (ndb_mgmd)
  cout << "connecting to mgmd ..." << flush;
  const int retries = 0;  // number of retries (< 0 = indefinitely)
  const int delay = 0;    // seconds to wait after retry
  const int verbose = 1;  // print report of progress
  // returns: 0 = success, 1 = recoverable error, -1 = non-recoverable error
  if (mgmd->connect(retries, delay, verbose) != 0)
    ABORT_ERROR("mgmd@" << mgmdConnect << " was not ready within "
                        << (retries * delay) << "s.");
  cout << "          [ok: " << mgmdConnect << "]" << endl;

  // optionally, connect and wait for reaching the data nodes (ndbds)
  cout << "waiting for data nodes ..." << flush;
  const int initial_wait = 10;  // seconds to wait until first node detected
  const int final_wait = 0;     // seconds to wait after first node detected
  // returns: 0 all nodes live, > 0 at least one node live, < 0 error
  if (mgmd->wait_until_ready(initial_wait, final_wait) < 0)
    ABORT_ERROR("data nodes were not ready within "
                << (initial_wait + final_wait) << "s.");
  cout << "      [ok]" << endl;

  // connect to database
  cout << "connecting to database ..." << flush;
  ndb = new Ndb(mgmd, catalog.c_str(), schema.c_str());
  const int max_no_tx = 10;  // maximum number of parallel tx (<=1024)
  // note each scan or index scan operation uses one extra transaction
  // if (ndb->init() != 0)
  if (ndb->init(max_no_tx) != 0) ABORT_NDB_ERROR(ndb->getNdbError());
  cout << "      [ok: " << catalog << "." << schema << "]" << endl;

  cout << "caching metadata ..." << flush;
  model = new NdbapiTwsModel(ndb);
  cout << "            [ok]" << endl;

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

void NdbapiTwsDriver::closeConnection() {
  assert(mgmd != NULL);
  assert(ndb != NULL);
  assert(tx == NULL);
  assert(model != NULL);

  cout << endl;

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

void NdbapiTwsDriver::clearData() {
  // not implemented yet
}

//---------------------------------------------------------------------------

void NdbapiTwsDriver::ndbapiToBuffer1blp(void *to, const char *from,
                                         size_t width) {
  char *t = (char *)to;
  size_t n = strlen(from);
  assert(0 <= n && n < width && width < 256);  // width <= 256 ?

  memcpy(t + 1, from, n);
  *t = n;
}

void NdbapiTwsDriver::ndbapiToString1blp(char *to, const void *from,
                                         size_t width) {
  const char *s = (const char *)from;
  size_t n = *s;
  assert(0 <= n && n < width && width < 256);  // width <= 256 ?

  memcpy(to, s + 1, n);
  to[n] = '\0';
}

// ---------------------------------------------------------------------------

int main(int argc, const char *argv[]) {
  NdbapiTwsDriver::parseArguments(argc, argv);
  NdbapiTwsDriver d;
  d.run();

  return 0;
}

//---------------------------------------------------------------------------
