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

#include <BlockNumbers.h>
#include <HugoTransactions.hpp>
#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <NdbSqlUtil.hpp>
#include <NodeBitmask.hpp>
#include <UtilTransactions.hpp>
#include <Vector.hpp>
#include <cstring>
#include <signaldata/DumpStateOrd.hpp>
#include "portlib/NdbSleep.h"
#include "util/TlsKeyManager.hpp"

#define CHECK(b)                                                          \
  if (!(b)) {                                                             \
    g_err << "ERR: " << step->getName() << " failed on line " << __LINE__ \
          << endl;                                                        \
    result = NDBT_FAILED;                                                 \
    break;                                                                \
  }

#define CHECKRET(b)                                                       \
  if (!(b)) {                                                             \
    g_err << "ERR: " << step->getName() << " failed on line " << __LINE__ \
          << endl;                                                        \
    return NDBT_FAILED;                                                   \
  }

struct Attrib {
  bool indexCreated;
  int numAttribs;
  int attribs[1024];
  Attrib() {
    numAttribs = 0;
    indexCreated = false;
  }
};

class AttribList {
 public:
  AttribList() {}
  ~AttribList() {
    for (unsigned i = 0; i < attriblist.size(); i++) {
      delete attriblist[i];
    }
  }
  void buildAttribList(const NdbDictionary::Table *pTab);
  Vector<Attrib *> attriblist;
};

/**
 * TODO expose in ndbapi
 */
static bool isIndexable(const NdbDictionary::Column *col) {
  if (col == 0) return false;

  switch (col->getType()) {
    case NDB_TYPE_BIT:
    case NDB_TYPE_BLOB:
    case NDB_TYPE_TEXT:
      return false;
    default:
      return true;
  }
}

void AttribList::buildAttribList(const NdbDictionary::Table *pTab) {
  attriblist.clear();

  Attrib *attr;
  // Build attrib definitions that describes which attributes to build index
  // Try to build strange combinations, not just "all" or all PK's

  int i;

  for (i = 1; i <= pTab->getNoOfColumns(); i++) {
    attr = new Attrib;
    attr->numAttribs = i;
    for (int a = 0; a < i; a++) attr->attribs[a] = a;
    attriblist.push_back(attr);
  }
  int b = 0;
  for (i = pTab->getNoOfColumns() - 1; i > 0; i--) {
    attr = new Attrib;
    attr->numAttribs = i;
    b++;
    for (int a = 0; a < i; a++) attr->attribs[a] = a + b;
    attriblist.push_back(attr);
  }
  for (i = pTab->getNoOfColumns(); i > 0; i--) {
    attr = new Attrib;
    attr->numAttribs = pTab->getNoOfColumns() - i;
    for (int a = 0; a < pTab->getNoOfColumns() - i; a++)
      attr->attribs[a] = pTab->getNoOfColumns() - a - 1;
    attriblist.push_back(attr);
  }
  for (i = 1; i < pTab->getNoOfColumns(); i++) {
    attr = new Attrib;
    attr->numAttribs = pTab->getNoOfColumns() - i;
    for (int a = 0; a < pTab->getNoOfColumns() - i; a++)
      attr->attribs[a] = pTab->getNoOfColumns() - a - 1;
    attriblist.push_back(attr);
  }
  for (i = 1; i < pTab->getNoOfColumns(); i++) {
    attr = new Attrib;
    attr->numAttribs = 2;
    for (int a = 0; a < 2; a++) {
      attr->attribs[a] = i % pTab->getNoOfColumns();
    }
    attriblist.push_back(attr);
  }

  // Last
  attr = new Attrib;
  attr->numAttribs = 1;
  attr->attribs[0] = pTab->getNoOfColumns() - 1;
  attriblist.push_back(attr);

  // Last and first
  attr = new Attrib;
  attr->numAttribs = 2;
  attr->attribs[0] = pTab->getNoOfColumns() - 1;
  attr->attribs[1] = 0;
  attriblist.push_back(attr);

  // First and last
  attr = new Attrib;
  attr->numAttribs = 2;
  attr->attribs[0] = 0;
  attr->attribs[1] = pTab->getNoOfColumns() - 1;
  attriblist.push_back(attr);

#if 0
  for(size_t i = 0; i < attriblist.size(); i++){

    ndbout << attriblist[i]->numAttribs << ": " ;
    for(int a = 0; a < attriblist[i]->numAttribs; a++)
      ndbout << attriblist[i]->attribs[a] << ", ";
    ndbout << endl;
  }
#endif

  /**
   * Trim away combinations that contain non indexable columns
   */
  Vector<Attrib *> tmp;
  for (Uint32 ii = 0; ii < attriblist.size(); ii++) {
    Attrib *attr = attriblist[ii];
    for (int j = 0; j < attr->numAttribs; j++) {
      if (!isIndexable(pTab->getColumn(attr->attribs[j]))) {
        delete attr;
        goto skip;
      }
    }

    if (attr->numAttribs + pTab->getNoOfPrimaryKeys() >
        NDB_MAX_ATTRIBUTES_IN_INDEX) {
      delete attr;
      goto skip;
    }

    tmp.push_back(attr);
  skip:
    (void)1;
  }

  attriblist.clear();
  attriblist = tmp;
}

char idxName[255];
char pkIdxName[255];

static const int SKIP_INDEX = 99;

int create_index(NDBT_Context *ctx, int indxNum,
                 const NdbDictionary::Table *pTab, Ndb *pNdb, Attrib *attr,
                 bool logged) {
  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);
  bool notOnlyPkId = ctx->getProperty("NotOnlyPkId", (unsigned)0);
  bool notIncludingUpdates =
      ctx->getProperty("NotIncludingUpdates", (unsigned)0);
  int result = NDBT_OK;

  HugoCalculator calc(*pTab);

  if (attr->numAttribs == 1 && calc.isUpdateCol(attr->attribs[0]) == true) {
    // Don't create index for the Hugo update column
    // since it's not unique
    return SKIP_INDEX;
  }

  // Create index
  BaseString::snprintf(idxName, 255, "IDC%d", indxNum);
  if (orderedIndex)
    ndbout << "Creating " << ((logged) ? "logged " : "temporary ")
           << "ordered index " << idxName << " (";
  else
    ndbout << "Creating " << ((logged) ? "logged " : "temporary ")
           << "unique index " << idxName << " (";
  ndbout << flush;
  NdbDictionary::Index pIdx(idxName);
  pIdx.setTable(pTab->getName());
  if (orderedIndex)
    pIdx.setType(NdbDictionary::Index::OrderedIndex);
  else
    pIdx.setType(NdbDictionary::Index::UniqueHashIndex);

  bool includesOnlyPkIdCols = true;
  for (int c = 0; c < attr->numAttribs; c++) {
    int attrNo = attr->attribs[c];
    const NdbDictionary::Column *col = pTab->getColumn(attrNo);
    switch (col->getType()) {
      case NDB_TYPE_BIT:
      case NDB_TYPE_BLOB:
      case NDB_TYPE_TEXT:
        /* Not supported */
        ndbout << col->getName() << " - bad type )" << endl;
        return SKIP_INDEX;
      default:
        break;
    }
    if (col->getStorageType() == NDB_STORAGETYPE_DISK) {
      ndbout << col->getName() << " - disk based )" << endl;
      return SKIP_INDEX;
    }
    if (calc.isUpdateCol(attrNo) && notIncludingUpdates) {
      ndbout << col->getName() << " - updates col, not including" << endl;
      return SKIP_INDEX;
    }

    pIdx.addIndexColumn(col->getName());
    ndbout << col->getName() << " ";

    if (!(col->getPrimaryKey() || calc.isIdCol(attrNo)))
      includesOnlyPkIdCols = false;
  }

  if (notOnlyPkId && includesOnlyPkIdCols) {
    ndbout << " Only PK/id cols included - skipping" << endl;
    return SKIP_INDEX;
  }

  if (!orderedIndex) {
    /**
     * For unique indexes we must add PK, otherwise it's not guaranteed
     *  to be unique
     */
    for (int i = 0; i < pTab->getNoOfColumns(); i++) {
      if (pTab->getColumn(i)->getPrimaryKey()) {
        for (int j = 0; j < attr->numAttribs; j++) {
          if (attr->attribs[j] == i) goto next;
        }
        pIdx.addIndexColumn(pTab->getColumn(i)->getName());
        ndbout << pTab->getColumn(i)->getName() << " ";
      }
    next:
      (void)i;
    }
  }

  pIdx.setStoredIndex(logged);
  ndbout << ") ";
  bool noddl = ctx->getProperty("NoDDL");

  if (noddl) {
    const NdbDictionary::Index *idx =
        pNdb->getDictionary()->getIndex(pIdx.getName(), pTab->getName());

    if (!idx) {
      ndbout << "Failed - Index does not exist and DDL not allowed" << endl;
      return NDBT_FAILED;
    } else {
      attr->indexCreated = false;
      // TODO : Check index definition is ok
    }
  } else {
    if (pNdb->getDictionary()->createIndex(pIdx) != 0) {
      attr->indexCreated = false;
      ndbout << "FAILED!" << endl;
      const NdbError err = pNdb->getDictionary()->getNdbError();
      NDB_ERR(err);
      if (err.classification == NdbError::ApplicationError) return SKIP_INDEX;

      if (err.status == NdbError::TemporaryError) return SKIP_INDEX;

      return NDBT_FAILED;
    } else {
      ndbout << "OK!" << endl;
      attr->indexCreated = true;
    }
  }
  return result;
}

int drop_index(int indxNum, Ndb *pNdb, const NdbDictionary::Table *pTab,
               Attrib *attr) {
  int result = NDBT_OK;

  if (attr->indexCreated == false) return NDBT_OK;

  BaseString::snprintf(idxName, 255, "IDC%d", indxNum);

  // Drop index
  ndbout << "Dropping index " << idxName << "(" << pTab->getName() << ") ";
  if (pNdb->getDictionary()->dropIndex(idxName, pTab->getName()) != 0) {
    ndbout << "FAILED!" << endl;
    NDB_ERR(pNdb->getDictionary()->getNdbError());
    result = NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }
  return result;
}

int runCreateIndexes(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  int l = 0;
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  int result = NDBT_OK;
  // NOTE If we need to test creating both logged and non logged indexes
  // this should be divided into two testcases
  // The parameter logged should then be specified
  // as a TC_PROPERTY. ex TC_PROPERTY("LoggedIndexes", 1);
  // and read into the test step like
  bool logged = ctx->getProperty("LoggedIndexes", 1);

  AttribList attrList;
  attrList.buildAttribList(pTab);

  while (l < loops && result == NDBT_OK) {
    unsigned int i;
    for (i = 0; i < attrList.attriblist.size(); i++) {
      // Try to create index
      if (create_index(ctx, i, pTab, pNdb, attrList.attriblist[i], logged) ==
          NDBT_FAILED)
        result = NDBT_FAILED;
    }

    // Now drop all indexes that where created
    for (i = 0; i < attrList.attriblist.size(); i++) {
      // Try to drop index
      if (drop_index(i, pNdb, pTab, attrList.attriblist[i]) != NDBT_OK)
        result = NDBT_FAILED;
    }

    l++;
  }

  return result;
}

int createRandomIndex(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  bool logged = ctx->getProperty("LoggedIndexes", 1);

  AttribList attrList;
  attrList.buildAttribList(pTab);

  int retries = 100;
  while (retries > 0) {
    const Uint32 i = rand() % attrList.attriblist.size();
    int res = create_index(ctx, i, pTab, pNdb, attrList.attriblist[i], logged);
    if (res == SKIP_INDEX) {
      retries--;
      continue;
    }

    if (res == NDBT_FAILED) {
      return NDBT_FAILED;
    }

    ctx->setProperty("createRandomIndex", i);
    // Now drop all indexes that where created

    return NDBT_OK;
  }

  return NDBT_FAILED;
}

int createRandomIndex_Drop(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);

  Uint32 i = ctx->getProperty("createRandomIndex");

  BaseString::snprintf(idxName, 255, "IDC%d", i);

  // Drop index
  ndbout << "Dropping index " << idxName << " ";
  if (pNdb->getDictionary()->dropIndex(idxName, ctx->getTab()->getName()) !=
      0) {
    ndbout << "FAILED!" << endl;
    NDB_ERR(pNdb->getDictionary()->getNdbError());
    return NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }

  return NDBT_OK;
}

int createPkIndex(NDBT_Context *ctx, NDBT_Step *step) {
  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);

  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);

  bool logged = ctx->getProperty("LoggedIndexes", 1);
  bool noddl = ctx->getProperty("NoDDL");

  // Create index
  BaseString::snprintf(pkIdxName, 255, "IDC_PK_%s", pTab->getName());
  if (orderedIndex)
    ndbout << "Creating " << ((logged) ? "logged " : "temporary ")
           << "ordered index " << pkIdxName << " (";
  else
    ndbout << "Creating " << ((logged) ? "logged " : "temporary ")
           << "unique index " << pkIdxName << " (";

  NdbDictionary::Index pIdx(pkIdxName);
  pIdx.setTable(pTab->getName());
  if (orderedIndex)
    pIdx.setType(NdbDictionary::Index::OrderedIndex);
  else
    pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
  for (int c = 0; c < pTab->getNoOfColumns(); c++) {
    const NdbDictionary::Column *col = pTab->getColumn(c);
    if (col->getPrimaryKey()) {
      pIdx.addIndexColumn(col->getName());
      ndbout << col->getName() << " ";
    }
  }

  pIdx.setStoredIndex(logged);
  ndbout << ") ";
  if (noddl) {
    const NdbDictionary::Index *idx =
        pNdb->getDictionary()->getIndex(pkIdxName, pTab->getName());

    if (!idx) {
      ndbout << "Failed - Index does not exist and DDL not allowed" << endl;
      NDB_ERR(pNdb->getDictionary()->getNdbError());
      return NDBT_FAILED;
    } else {
      // TODO : Check index definition is ok
    }
  } else {
    if (pNdb->getDictionary()->createIndex(pIdx) != 0) {
      ndbout << "FAILED!" << endl;
      const NdbError err = pNdb->getDictionary()->getNdbError();
      NDB_ERR(err);
      return NDBT_FAILED;
    }
  }

  ndbout << "OK!" << endl;
  return NDBT_OK;
}

int createPkIndex_Drop(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);

  bool noddl = ctx->getProperty("NoDDL");

  // Drop index
  if (!noddl) {
    ndbout << "Dropping index " << pkIdxName << " ";
    if (pNdb->getDictionary()->dropIndex(pkIdxName, pTab->getName()) != 0) {
      ndbout << "FAILED!" << endl;
      NDB_ERR(pNdb->getDictionary()->getNdbError());
      return NDBT_FAILED;
    } else {
      ndbout << "OK!" << endl;
    }
  }

  return NDBT_OK;
}

int runVerifyIndex(NDBT_Context *ctx, NDBT_Step *step) {
  // Verify that data in index match
  // table data
  Ndb *pNdb = GETNDB(step);
  UtilTransactions utilTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 16);
  const int parallelism = batchSize > 240 ? 240 : batchSize;

  do {
    if (utilTrans.verifyIndex(pNdb, idxName, parallelism, true) != 0) {
      g_err << "Inconsistent index" << endl;
      return NDBT_FAILED;
    }
  } while (ctx->isTestStopped() == false);
  return NDBT_OK;
}

int runTransactions1(NDBT_Context *ctx, NDBT_Step *step) {
  // Verify that data in index match
  // table data
  Ndb *pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 50);

  int rows = ctx->getNumRecords();
  while (ctx->isTestStopped() == false) {
    if (hugoTrans.pkUpdateRecords(pNdb, rows, batchSize) != 0) {
      g_err << "Updated table failed" << endl;
      return NDBT_FAILED;
    }

    ctx->sync_down("PauseThreads");
    if (ctx->isTestStopped()) break;

    if (hugoTrans.scanUpdateRecords(pNdb, rows, batchSize) != 0) {
      g_err << "Updated table failed" << endl;
      return NDBT_FAILED;
    }

    ctx->sync_down("PauseThreads");
  }
  return NDBT_OK;
}

int runTransactions2(NDBT_Context *ctx, NDBT_Step *step) {
  // Verify that data in index match
  // table data
  Ndb *pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 50);

  int rows = ctx->getNumRecords();
  while (ctx->isTestStopped() == false) {
#if 1
    if (hugoTrans.indexReadRecords(pNdb, pkIdxName, rows, batchSize) != 0) {
      g_err << "Index read failed" << endl;
      return NDBT_FAILED;
    }
#endif
    ctx->sync_down("PauseThreads");
    if (ctx->isTestStopped()) break;
#if 1
    if (hugoTrans.indexUpdateRecords(pNdb, pkIdxName, rows, batchSize) != 0) {
      g_err << "Index update failed" << endl;
      return NDBT_FAILED;
    }
#endif
    ctx->sync_down("PauseThreads");
  }
  return NDBT_OK;
}

int runTransactions3(NDBT_Context *ctx, NDBT_Step *step) {
  // Verify that data in index match
  // table data
  Ndb *pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 32);
  const int parallel = batchSize > 240 ? 240 : batchSize;

  int rows = ctx->getNumRecords();
  while (ctx->isTestStopped() == false) {
    if (hugoTrans.loadTable(pNdb, rows, batchSize, false) != 0) {
      g_err << "Load table failed" << endl;
      return NDBT_FAILED;
    }
    ctx->sync_down("PauseThreads");
    if (ctx->isTestStopped()) break;

    if (hugoTrans.pkUpdateRecords(pNdb, rows, batchSize) != 0) {
      g_err << "Updated table failed" << endl;
      return NDBT_FAILED;
    }

    ctx->sync_down("PauseThreads");
    if (ctx->isTestStopped()) break;

    if (hugoTrans.indexReadRecords(pNdb, pkIdxName, rows, batchSize) != 0) {
      g_err << "Index read failed" << endl;
      return NDBT_FAILED;
    }

    ctx->sync_down("PauseThreads");
    if (ctx->isTestStopped()) break;

    if (hugoTrans.indexUpdateRecords(pNdb, pkIdxName, rows, batchSize) != 0) {
      g_err << "Index update failed" << endl;
      return NDBT_FAILED;
    }

    ctx->sync_down("PauseThreads");
    if (ctx->isTestStopped()) break;

    if (hugoTrans.scanUpdateRecords(pNdb, rows, 5, parallel) != 0) {
      g_err << "Scan updated table failed" << endl;
      return NDBT_FAILED;
    }

    ctx->sync_down("PauseThreads");
    if (ctx->isTestStopped()) break;

    if (utilTrans.verifyIndex(pNdb, idxName, parallel) != 0) {
      g_err << "Inconsistent index" << endl;
      return NDBT_FAILED;
    }
    if (utilTrans.clearTable(pNdb, rows, parallel) != 0) {
      g_err << "Clear table failed" << endl;
      return NDBT_FAILED;
    }
    if (utilTrans.verifyIndex(pNdb, idxName, parallel) != 0) {
      g_err << "Inconsistent index" << endl;
      return NDBT_FAILED;
    }

    ctx->sync_down("PauseThreads");
    if (ctx->isTestStopped()) break;

    int count = -1;
    if (utilTrans.selectCount(pNdb, 64, &count) != 0 || count != 0)
      return NDBT_FAILED;
    ctx->sync_down("PauseThreads");
  }
  return NDBT_OK;
}

int runRestarts(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NDBT_TestCase *pCase = ctx->getCase();
  NdbRestarts restarts;
  int i = 0;
  int timeout = 240;
  int sync_threads = ctx->getProperty("Threads", (unsigned)0);

  while (i < loops && result != NDBT_FAILED && !ctx->isTestStopped()) {
    if (restarts.executeRestart(ctx, "RestartRandomNodeAbort", timeout) != 0) {
      g_err << "Failed to executeRestart(" << pCase->getName() << ")" << endl;
      result = NDBT_FAILED;
      break;
    }
    ctx->sync_up_and_wait("PauseThreads", sync_threads);
    i++;
  }
  ctx->stopTest();
  return result;
}

int runCreateLoadDropIndex(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int l = 0;
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  int result = NDBT_OK;
  int batchSize = ctx->getProperty("BatchSize", 1);
  int parallelism = batchSize > 240 ? 240 : batchSize;
  ndbout << "batchSize=" << batchSize << endl;
  bool logged = ctx->getProperty("LoggedIndexes", 1);

  HugoTransactions hugoTrans(*pTab);
  UtilTransactions utilTrans(*pTab);
  AttribList attrList;
  attrList.buildAttribList(pTab);

  for (unsigned int i = 0; i < attrList.attriblist.size(); i++) {
    while (l < loops && result == NDBT_OK) {
      if ((l % 2) == 0) {
        // Create index first and then load

        // Try to create index
        if (create_index(ctx, i, pTab, pNdb, attrList.attriblist[i], logged) ==
            NDBT_FAILED) {
          result = NDBT_FAILED;
        }

        // Load the table with data
        ndbout << "Loading data after" << endl;
        CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);

      } else {
        // Load table then create index

        // Load the table with data
        ndbout << "Loading data before" << endl;
        CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);

        // Try to create index
        if (create_index(ctx, i, pTab, pNdb, attrList.attriblist[i], logged) ==
            NDBT_FAILED)
          result = NDBT_FAILED;
      }

      // Verify that data in index match
      // table data
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);

      // Do it all...
      ndbout << "Doing it all" << endl;
      int count;
      ndbout << "  pkUpdateRecords" << endl;
      CHECK(hugoTrans.pkUpdateRecords(pNdb, records, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      CHECK(hugoTrans.pkUpdateRecords(pNdb, records, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      ndbout << "  pkDelRecords half" << endl;
      CHECK(hugoTrans.pkDelRecords(pNdb, records / 2, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      ndbout << "  scanUpdateRecords" << endl;
      CHECK(hugoTrans.scanUpdateRecords(pNdb, records / 2, parallelism) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      ndbout << "  clearTable" << endl;
      CHECK(utilTrans.clearTable(pNdb, records / 2, parallelism) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == 0);
      ndbout << "  loadTable" << endl;
      CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      ndbout << "  loadTable again" << endl;
      CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == records);

      if ((l % 2) == 0) {
        // Drop index first and then clear

        // Try to create index
        if (drop_index(i, pNdb, pTab, attrList.attriblist[i]) != NDBT_OK) {
          result = NDBT_FAILED;
        }

        // Clear table
        ndbout << "Clearing table after" << endl;
        CHECK(hugoTrans.clearTable(pNdb, records, parallelism) == 0);

      } else {
        // Clear table then drop index

        // Clear table
        ndbout << "Clearing table before" << endl;
        CHECK(hugoTrans.clearTable(pNdb, records, parallelism) == 0);

        // Try to drop index
        if (drop_index(i, pNdb, pTab, attrList.attriblist[i]) != NDBT_OK)
          result = NDBT_FAILED;
      }

      ndbout << "  Done!" << endl;
      l++;
    }

    // Make sure index is dropped
    drop_index(i, pNdb, pTab, attrList.attriblist[i]);
  }

  return result;
}

int runInsertDelete(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  int result = NDBT_OK;
  int batchSize = ctx->getProperty("BatchSize", 1);
  int parallelism = batchSize > 240 ? 240 : batchSize;
  ndbout << "batchSize=" << batchSize << endl;
  bool logged = ctx->getProperty("LoggedIndexes", 1);

  HugoTransactions hugoTrans(*pTab);
  UtilTransactions utilTrans(*pTab);

  AttribList attrList;
  attrList.buildAttribList(pTab);

  for (unsigned int i = 0; i < attrList.attriblist.size(); i++) {
    Attrib *attr = attrList.attriblist[i];
    // Create index
    if (create_index(ctx, i, pTab, pNdb, attr, logged) == NDBT_OK) {
      int l = 1;
      while (l <= loops && result == NDBT_OK) {
        CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);
        CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
        CHECK(utilTrans.clearTable(pNdb, records, parallelism) == 0);
        CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
        l++;
      }

      // Drop index
      if (drop_index(i, pNdb, pTab, attr) != NDBT_OK) result = NDBT_FAILED;
    }
  }

  return result;
}

int tryAddUniqueIndex(Ndb *pNdb, const NdbDictionary::Table *pTab,
                      const char *idxName, HugoCalculator &calc,
                      int &chosenCol) {
  for (int c = 0; c < pTab->getNoOfColumns(); c++) {
    const NdbDictionary::Column *col = pTab->getColumn(c);

    if (!col->getPrimaryKey() && !calc.isUpdateCol(c) && !col->getNullable() &&
        col->getStorageType() != NDB_STORAGETYPE_DISK) {
      chosenCol = c;
      break;
    }
  }

  if (chosenCol == -1) {
    return 1;
  }

  /* Create unique index on chosen column */

  const char *colName = pTab->getColumn(chosenCol)->getName();
  ndbout << "Creating unique index :" << idxName << " on (" << colName << ")"
         << endl;

  NdbDictionary::Index idxDef(idxName);
  idxDef.setTable(pTab->getName());
  idxDef.setType(NdbDictionary::Index::UniqueHashIndex);

  idxDef.addIndexColumn(colName);
  idxDef.setStoredIndex(false);

  if (pNdb->getDictionary()->createIndex(idxDef) != 0) {
    ndbout << "FAILED!" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    NDB_ERR(err);
    return -1;
  }

  return 0;
}

int tryInsertUniqueRecord(NDBT_Step *step, HugoOperations &hugoOps,
                          int &recordNum) {
  Ndb *pNdb = GETNDB(step);
  do {
    CHECKRET(hugoOps.startTransaction(pNdb) == 0);
    CHECKRET(hugoOps.pkInsertRecord(pNdb, recordNum,
                                    1,  // NumRecords
                                    0)  // UpdatesValue
             == 0);
    if (hugoOps.execute_Commit(pNdb) != 0) {
      NdbError err = hugoOps.getTransaction()->getNdbError();
      hugoOps.closeTransaction(pNdb);
      if (err.code == 839) {
        /* Unique constraint violation, try again with
         * different record
         */
        recordNum++;
        continue;
      } else {
        NDB_ERR(err);
        return NDBT_FAILED;
      }
    }

    hugoOps.closeTransaction(pNdb);
    break;
  } while (true);

  return NDBT_OK;
}

int runConstraintDetails(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);

  /* Steps in testcase
   * 1) Choose a column to index - not pk or updates column
   * 2) Insert a couple of unique rows
   * 3) For a number of different batch sizes :
   *    i)  Insert a row with a conflicting values
   *    ii) Update an existing row with a conflicting value
   *    Verify :
   *    - The correct error is received
   *    - The failing constraint is detected
   *    - The error details string is as expected.
   */
  HugoCalculator calc(*pTab);

  /* Choose column to add unique index to */

  int chosenCol = -1;
  const char *idxName = "constraintCheck";

  int rc = tryAddUniqueIndex(pNdb, pTab, idxName, calc, chosenCol);

  if (rc) {
    if (rc == 1) {
      ndbout << "No suitable column in this table, skipping" << endl;
      return NDBT_OK;
    }
    return NDBT_FAILED;
  }

  const NdbDictionary::Index *pIdx =
      pNdb->getDictionary()->getIndex(idxName, pTab->getName());
  CHECKRET(pIdx != 0);

  /* Now insert a couple of rows */

  HugoOperations hugoOps(*pTab);
  int firstRecordNum = 0;
  CHECKRET(tryInsertUniqueRecord(step, hugoOps, firstRecordNum) == NDBT_OK);
  int secondRecordNum = firstRecordNum + 1;
  CHECKRET(tryInsertUniqueRecord(step, hugoOps, secondRecordNum) == NDBT_OK);

  /* Now we'll attempt to insert/update records
   * in various sized batches and check the errors which
   * are returned
   */

  int maxBatchSize = 10;
  int recordOffset = secondRecordNum + 1;
  char buff[NDB_MAX_TUPLE_SIZE];
  Uint32 real_len;
  CHECKRET(calc.calcValue(firstRecordNum, chosenCol, 0, &buff[0],
                          pTab->getColumn(chosenCol)->getSizeInBytes(),
                          &real_len) != 0);

  for (int optype = 0; optype < 2; optype++) {
    bool useInsert = (optype == 0);
    ndbout << "Verifying constraint violation for "
           << (useInsert ? "Insert" : "Update") << " operations" << endl;

    for (int batchSize = 1; batchSize <= maxBatchSize; batchSize++) {
      NdbTransaction *trans = pNdb->startTransaction();
      CHECKRET(trans != 0);

      for (int rows = 0; rows < batchSize; rows++) {
        int rowId = recordOffset + rows;
        NdbOperation *op = trans->getNdbOperation(pTab);
        CHECKRET(op != 0);
        if (useInsert) {
          CHECKRET(op->insertTuple() == 0);

          CHECKRET(hugoOps.setValues(op, rowId, 0) == 0);

          /* Now override setValue for the indexed column to cause
           * constraint violation
           */
          CHECKRET(op->setValue(chosenCol, &buff[0], real_len) == 0);
        } else {
          /* Update value of 'second' row to conflict with
           * first
           */
          CHECKRET(op->updateTuple() == 0);
          CHECKRET(hugoOps.equalForRow(op, secondRecordNum) == 0);

          CHECKRET(op->setValue(chosenCol, &buff[0], real_len) == 0);
        }
      }

      CHECKRET(trans->execute(Commit) == -1);

      NdbError err = trans->getNdbError();

      NDB_ERR(err);

      CHECKRET(err.code == 893);

      /* Ugliness - current NdbApi puts index schema object id
       * as abs. value of char* in NdbError struct
       */

      int idxObjId = (int)((UintPtr)err.details - UintPtr(0));
      char detailsBuff[100];
      const char *errIdxName = NULL;

      ndbout_c("Got details column val of %p and string of %s\n", err.details,
               pNdb->getNdbErrorDetail(err, &detailsBuff[0], 100));
      if (idxObjId == pIdx->getObjectId()) {
        /* Insert / update failed on the constraint we added */
        errIdxName = pIdx->getName();
      } else {
        /* We failed on a different constraint.
         * Some NDBT tables already have constraints (e.g. I3)
         * Check that the failing constraint contains our column
         */
        NdbDictionary::Dictionary::List tableIndices;

        CHECKRET(pNdb->getDictionary()->listIndexes(tableIndices,
                                                    pTab->getName()) == 0);

        bool ok = false;
        for (unsigned ind = 0; ind < tableIndices.count; ind++) {
          if (tableIndices.elements[ind].id == (unsigned)idxObjId) {
            const char *otherIdxName = tableIndices.elements[ind].name;
            ndbout << "Found other violated constraint : " << otherIdxName
                   << endl;
            const NdbDictionary::Index *otherIndex =
                pNdb->getDictionary()->getIndex(otherIdxName, pTab->getName());
            CHECKRET(otherIndex != NULL);

            for (unsigned col = 0; col < otherIndex->getNoOfColumns(); col++) {
              if (strcmp(otherIndex->getColumn(col)->getName(),
                         pTab->getColumn(chosenCol)->getName()) == 0) {
                /* Found our column in the index */
                ok = true;
                errIdxName = otherIndex->getName();
                break;
              }
            }

            if (ok) {
              ndbout << "  Constraint contains unique column " << endl;
              break;
            }
            ndbout << "  Constraint does not contain unique col - fail" << endl;
            CHECKRET(false);
          }
        }

        if (!ok) {
          ndbout << "Did not find violated constraint" << endl;
          CHECKRET(false);
        }
      }

      /* Finally verify the name returned is :
       * <db>/<schema>/<table>/<index>
       */
      BaseString expected;

      expected.assfmt("%s/%s/%s/%s", pNdb->getDatabaseName(),
                      pNdb->getSchemaName(), pTab->getName(), errIdxName);

      CHECKRET(strcmp(expected.c_str(), &detailsBuff[0]) == 0);

      ndbout << " OK " << endl;

      trans->close();
    }
  }

  return NDBT_OK;
}

int runLoadTable(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();

  HugoTransactions hugoTrans(*ctx->getTab());
  int batchSize = ctx->getProperty("BatchSize", 1);
  if (hugoTrans.loadTable(GETNDB(step), records, batchSize) != 0) {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTable(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();

  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable(GETNDB(step), records) != 0) {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runSystemRestart1(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int count;
  NdbRestarter restarter;
  Uint32 i = 1;

  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  while (i <= loops && result != NDBT_FAILED) {
    ndbout << "Loop " << i << "/" << loops << " started" << endl;
    /*
      1. Load data
      2. Restart cluster and verify records
      3. Update records
      4. Restart cluster and verify records
      5. Delete half of the records
      6. Restart cluster and verify records
      7. Delete all records
      8. Restart cluster and verify records
      9. Insert, update, delete records
      10. Restart cluster and verify records
      11. Insert, update, delete records
      12. Restart cluster with error insert 5020 and verify records
    */
    ndbout << "Loading records..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Updating records..." << endl;
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Deleting 50% of records..." << endl;
    CHECK(hugoTrans.pkDelRecords(pNdb, records / 2) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.scanReadRecords(pNdb, records / 2, 0, 64) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records / 2));
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Deleting all records..." << endl;
    CHECK(utilTrans.clearTable(pNdb, records / 2) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Doing it all..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(hugoTrans.pkDelRecords(pNdb, records / 2) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records / 2) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);

    ndbout << "Doing it all..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(hugoTrans.pkDelRecords(pNdb, records / 2) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records / 2) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);

    ndbout << "Restarting cluster with error insert 5020..." << endl;
    CHECK(restarter.restartAll(false, true) == 0);
    CHECK(restarter.waitClusterNoStart(timeout) == 0);
    CHECK(restarter.insertErrorInAllNodes(5020) == 0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Clear error insert 5020" << endl;
    CHECK(restarter.insertErrorInAllNodes(0) == 0);
    i++;
  }

  ctx->stopTest();
  ndbout << "runSystemRestart1 finished" << endl;

  return result;
}

#define CHECK2(b, t)                        \
  if (!(b)) {                               \
    g_err << __LINE__ << ": " << t << endl; \
    break;                                  \
  }
#define CHECKOKORTIMEOUT(e, t)                                          \
  {                                                                     \
    int rc = (e);                                                       \
    if (rc != 0) {                                                      \
      if (rc == 266) {                                                  \
        g_err << "Timeout : retries left : " << timeoutRetries << endl; \
        continue;                                                       \
      }                                                                 \
      g_err << __LINE__ << ": " << (t) << endl;                         \
      break;                                                            \
    }                                                                   \
  }

int runMixed1(NDBT_Context *ctx, NDBT_Step *step) {
  // Verify that data in index match
  // table data
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());

  /* Old, rather ineffective testcase which nonetheless passes on 6.3 */

  do {
    // TC1
    g_err << "pkRead, indexRead, Commit" << endl;
    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0,
           "indexReadRecords");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecord");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");
    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");

    // TC1
    g_err << "pkRead, indexRead, Commit" << endl;
    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecord");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0,
           "indexReadRecords");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");
    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");

    // TC2
    g_err << "pkRead, indexRead, NoCommit, Commit" << endl;
    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecord");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0,
           "indexReadRecords");
    CHECK2(hugoOps.execute_NoCommit(pNdb) == 0, "executeNoCommit");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");
    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");

    // TC3
    g_err << "pkRead, pkRead, Commit" << endl;
    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction ");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecords ");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecords ");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");
    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction ");

    // TC4
    g_err << "indexRead, indexRead, Commit" << endl;

    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction ");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0,
           "indexReadRecords");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0,
           "indexReadRecords");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");

    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction ");

    return NDBT_OK;
  } while (false);

  hugoOps.closeTransaction(pNdb);
  return NDBT_FAILED;
}

int runMixedUpdateInterleaved(Ndb *pNdb, HugoOperations &hugoOps,
                              int outOfRangeRec, int testSize, bool commit,
                              bool abort, int pkFailRec, int ixFailRec,
                              bool invertFail, AbortOption ao, int whatToUpdate,
                              int updatesValue, bool ixFirst) {
  int execRc = 0;
  if ((pkFailRec != -1) || (ixFailRec != -1)) {
    execRc = 626;
  }

  bool updateViaPk = whatToUpdate & 1;
  bool updateViaIx = whatToUpdate & 2;

  int ixOpNum = (ixFirst ? 0 : 1);
  int pkOpNum = (ixFirst ? 1 : 0);

  int timeoutRetries = 3;

  while (timeoutRetries--) {
    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
    for (int i = 0; i < testSize; i++) {
      /* invertFail causes all issued reads *except* the fail record number
       * to fail
       */
      int indxKey = ((i == ixFailRec) ^ invertFail) ? outOfRangeRec : i;
      int pkKey = ((i == pkFailRec) ^ invertFail) ? outOfRangeRec : i;

      for (int opNum = 0; opNum < 2; opNum++) {
        if (opNum == ixOpNum) {
          if (updateViaIx) {
            CHECK2(hugoOps.indexUpdateRecord(pNdb, pkIdxName, indxKey, 1,
                                             updatesValue) == 0,
                   "indexUpdateRecord");
          } else {
            CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, indxKey) == 0,
                   "indexReadRecords");
          }
        }

        if (opNum == pkOpNum) {
          if (updateViaPk) {
            CHECK2(hugoOps.pkUpdateRecord(pNdb, pkKey, 1, updatesValue) == 0,
                   "pkUpdateRecord");
          } else {
            CHECK2(hugoOps.pkReadRecord(pNdb, pkKey) == 0, "pkReadRecord");
          }
        }
      }
    }
    if (commit) {
      int rc = hugoOps.execute_Commit(pNdb, ao);
      if (rc == 266) {
        /* Timeout */
        g_err << "Timeout : retries left=" << timeoutRetries << endl;
        hugoOps.closeTransaction(pNdb);
        continue;
      }
      CHECK2(rc == execRc, "execute_Commit");
      NdbError err = hugoOps.getTransaction()->getNdbError();
      CHECK2(err.code == execRc, "getNdbError");
    } else {
      int rc = hugoOps.execute_NoCommit(pNdb, ao);
      if (rc == 266) {
        /* Timeout */
        g_err << "Timeout : retries left=" << timeoutRetries << endl;
        hugoOps.closeTransaction(pNdb);
        continue;
      }
      CHECK2(rc == execRc, "execute_NoCommit");
      NdbError err = hugoOps.getTransaction()->getNdbError();
      CHECK2(err.code == execRc, "getNdbError");
      if (execRc && (ao == AO_IgnoreError)) {
        /* Transaction should still be open, let's commit it */
        CHECK2(hugoOps.execute_Commit(pNdb, ao) == 0, "executeCommit");
      } else if (abort) {
        CHECK2(hugoOps.execute_Rollback(pNdb) == 0, "executeRollback");
      }
    }
    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");

    return 1;
  }

  hugoOps.closeTransaction(pNdb);
  return 0;
}

int runMixed2(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());

  int numRecordsInTable = ctx->getNumRecords();
  const int maxTestSize = 10000;
  int testSize = MIN(numRecordsInTable, maxTestSize);

  /* Avoid overloading Send Buffers */
  Uint32 rowSize =
      NdbDictionary::getRecordRowLength(ctx->getTab()->getDefaultRecord());
  Uint32 dataXfer = 2 * rowSize * testSize;
  const Uint32 MaxDataXfer = 500000;  // 0.5M

  if (dataXfer > MaxDataXfer) {
    testSize = MIN((int)(MaxDataXfer / rowSize), testSize);
  }

  g_err << "testSize= " << testSize << endl;
  g_err << "rowSize= " << rowSize << endl;

  int updatesValue = 1;
  const int maxTimeoutRetries = 3;

  do {
    // TC0
    {
      bool ok = false;
      int timeoutRetries = maxTimeoutRetries;
      while (timeoutRetries--) {
        g_err << "TC0 : indexRead, pkread, Commit" << endl;
        CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
        CHECK2(
            hugoOps.indexReadRecords(pNdb, pkIdxName, 0, false, testSize) == 0,
            "indexReadRecords");
        CHECK2(hugoOps.pkReadRecord(pNdb, 0, testSize) == 0, "pkReadRecord");
        CHECKOKORTIMEOUT(hugoOps.execute_Commit(pNdb), "executeCommit");
        CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");

        ok = true;
        break;
      }
      if (!ok) {
        break;
      };
    }

    // TC1
    {
      bool ok = false;
      int timeoutRetries = maxTimeoutRetries;
      while (timeoutRetries--) {
        g_err << "TC1 : pkRead, indexRead, Commit" << endl;
        CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
        CHECK2(hugoOps.pkReadRecord(pNdb, 0, testSize) == 0, "pkReadRecord");
        CHECK2(
            hugoOps.indexReadRecords(pNdb, pkIdxName, 0, false, testSize) == 0,
            "indexReadRecords");
        CHECKOKORTIMEOUT(hugoOps.execute_Commit(pNdb), "executeCommit");
        CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");

        ok = true;
        break;
      }
      if (!ok) {
        break;
      };
    }

    // TC2
    {
      bool ok = false;
      int timeoutRetries = maxTimeoutRetries;
      while (timeoutRetries--) {
        g_err << "TC2 : pkRead, indexRead, NoCommit, Commit" << endl;
        CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
        CHECK2(hugoOps.pkReadRecord(pNdb, 0, testSize) == 0, "pkReadRecord");
        CHECK2(
            hugoOps.indexReadRecords(pNdb, pkIdxName, 0, false, testSize) == 0,
            "indexReadRecords");
        CHECKOKORTIMEOUT(hugoOps.execute_NoCommit(pNdb), "executeNoCommit");
        CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");
        CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");
        ok = true;
        break;
      }
      if (!ok) {
        break;
      };
    }

    // TC3
    {
      bool ok = false;
      int timeoutRetries = maxTimeoutRetries;
      while (timeoutRetries--) {
        g_err << "TC3 : pkRead, pkRead, Commit" << endl;
        CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction ");
        CHECK2(hugoOps.pkReadRecord(pNdb, 0, testSize) == 0, "pkReadRecords ");
        CHECK2(hugoOps.pkReadRecord(pNdb, 0, testSize) == 0, "pkReadRecords ");
        CHECKOKORTIMEOUT(hugoOps.execute_Commit(pNdb), "executeCommit");
        CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction ");
        ok = true;
        break;
      }
      if (!ok) {
        break;
      };
    }

    // TC4
    {
      bool ok = false;
      int timeoutRetries = maxTimeoutRetries;
      while (timeoutRetries--) {
        g_err << "TC4 : indexRead, indexRead, Commit" << endl;
        CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction ");
        CHECK2(
            hugoOps.indexReadRecords(pNdb, pkIdxName, 0, false, testSize) == 0,
            "indexReadRecords");
        CHECK2(
            hugoOps.indexReadRecords(pNdb, pkIdxName, 0, false, testSize) == 0,
            "indexReadRecords");
        CHECKOKORTIMEOUT(hugoOps.execute_Commit(pNdb), "executeCommit");
        CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction ");
        ok = true;
        break;
      }
      if (!ok) {
        break;
      };
    }

    // TC5
    {
      bool ok = false;
      int timeoutRetries = maxTimeoutRetries;
      while (timeoutRetries--) {
        g_err << "TC5 : indexRead, pkUpdate, Commit" << endl;
        CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
        CHECK2(
            hugoOps.indexReadRecords(pNdb, pkIdxName, 0, false, testSize) == 0,
            "indexReadRecords");
        CHECK2(hugoOps.pkUpdateRecord(pNdb, 0, testSize, updatesValue++) == 0,
               "pkUpdateRecord");
        CHECKOKORTIMEOUT(hugoOps.execute_Commit(pNdb), "executeCommit");
        CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");
        ok = true;
        break;
      }
      if (!ok) {
        break;
      };
    }

    // TC6
    {
      bool ok = false;
      int timeoutRetries = maxTimeoutRetries;
      while (timeoutRetries--) {
        g_err << "TC6 : pkUpdate, indexRead, Commit" << endl;
        CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
        CHECK2(hugoOps.pkUpdateRecord(pNdb, 0, testSize, updatesValue++) == 0,
               "pkUpdateRecord");
        CHECK2(
            hugoOps.indexReadRecords(pNdb, pkIdxName, 0, false, testSize) == 0,
            "indexReadRecords");
        CHECKOKORTIMEOUT(hugoOps.execute_Commit(pNdb), "executeCommit");
        CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");
        ok = true;
        break;
      }
      if (!ok) {
        break;
      };
    }

    // TC7
    {
      bool ok = false;
      int timeoutRetries = maxTimeoutRetries;
      while (timeoutRetries--) {
        g_err << "TC7 : pkRead, indexUpdate, Commit" << endl;
        CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
        CHECK2(hugoOps.pkReadRecord(pNdb, 0, testSize) == 0, "pkReadRecord");
        CHECK2(hugoOps.indexUpdateRecord(pNdb, pkIdxName, 0, testSize,
                                         updatesValue++) == 0,
               "indexReadRecords");
        CHECKOKORTIMEOUT(hugoOps.execute_Commit(pNdb), "executeCommit");
        CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");
        ok = true;
        break;
      }
      if (!ok) {
        break;
      };
    }

    // TC8
    {
      bool ok = false;
      int timeoutRetries = maxTimeoutRetries;
      while (timeoutRetries--) {
        g_err << "TC8 : indexUpdate, pkRead, Commit" << endl;
        CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction ");
        CHECK2(hugoOps.indexUpdateRecord(pNdb, pkIdxName, 0, testSize,
                                         updatesValue++) == 0,
               "indexReadRecords ");
        CHECK2(hugoOps.pkReadRecord(pNdb, 0, testSize) == 0, "pkReadRecords ");
        CHECKOKORTIMEOUT(hugoOps.execute_Commit(pNdb), "executeCommit");
        CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction ");
        ok = true;
        break;
      }
      if (!ok) {
        break;
      };
    }

    for (int ao = 0; ao < 2; ao++) {
      AbortOption abortOption = ao ? AO_IgnoreError : AbortOnError;

      for (int exType = 0; exType < 3; exType++) {
        bool commit = (exType == 1);
        bool abort = (exType == 2);

        const char *exTypeStr = ((exType == 0)   ? "NoCommit"
                                 : (exType == 1) ? "Commit"
                                                 : "Abort");

        for (int failType = 0; failType < 4; failType++) {
          for (int failPos = 0; failPos < 2; failPos++) {
            int failRec = (failPos == 0) ? 0 : testSize - 1;
            int pkFailRec = -1;
            int ixFailRec = -1;
            if (failType) {
              if (failType & 1) pkFailRec = failRec;
              if (failType & 2) ixFailRec = failRec;
            }

            for (int invFail = 0; invFail < ((failType == 0) ? 1 : 2);
                 invFail++) {
              bool invertFail = (invFail) ? true : false;
              const char *failTypeStr =
                  ((failType == 0)
                       ? "None"
                       : ((failType == 1) ? "Pk"
                                          : ((failType == 2) ? "Ix" : "Both")));
              for (int updateVia = 0; updateVia < 3; updateVia++) {
                const char *updateViaStr = ((updateVia == 0)   ? "None"
                                            : (updateVia == 1) ? "Pk"
                                            : (updateVia == 2) ? "Ix"
                                                               : "Both");
                for (int updateOrder = 0; updateOrder < 2; updateOrder++) {
                  bool updateIxFirst = (updateOrder == 0);
                  g_err << endl
                        << "AbortOption : "
                        << (ao ? "IgnoreError" : "AbortOnError") << endl
                        << "ExecType : " << exTypeStr << endl
                        << "Failtype : " << failTypeStr << endl
                        << "Failpos : " << ((failPos == 0) ? "Early" : "Late")
                        << endl
                        << "Failure scenarios : "
                        << (invFail ? "All but one" : "one") << endl
                        << "UpdateVia : " << updateViaStr << endl
                        << "Order : "
                        << (updateIxFirst ? "Index First" : "Pk first") << endl;
                  bool ok = false;
                  do {
                    g_err << "Mixed read/update interleaved" << endl;
                    CHECK2(runMixedUpdateInterleaved(
                               pNdb, hugoOps, numRecordsInTable, testSize,
                               commit,      // Commit
                               abort,       // Abort
                               pkFailRec,   // PkFail
                               ixFailRec,   // IxFail
                               invertFail,  // Invertfail
                               abortOption, updateVia, updatesValue++,
                               updateIxFirst),
                           "TC4");

                    ok = true;
                  } while (false);

                  if (!ok) {
                    hugoOps.closeTransaction(pNdb);
                    return NDBT_FAILED;
                  }
                }
              }
            }
          }
        }
      }
    }

    return NDBT_OK;
  } while (false);

  hugoOps.closeTransaction(pNdb);
  return NDBT_FAILED;
}

#define check(b, e)                                                       \
  if (!(b)) {                                                             \
    g_err << "ERR: " << step->getName() << " failed on line " << __LINE__ \
          << ": " << e.getNdbError() << endl;                             \
    return NDBT_FAILED;                                                   \
  }

int runRefreshTupleAbort(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  int loops = ctx->getNumLoops();

  Ndb *ndb = GETNDB(step);

  const NdbDictionary::Table &tab = *ctx->getTab();

  for (int i = 0; i < tab.getNoOfColumns(); i++) {
    if (tab.getColumn(i)->getStorageType() == NDB_STORAGETYPE_DISK) {
      g_err << "Table has disk column(s) skipping." << endl;
      return NDBT_OK;
    }
  }

  g_err << "Loading table." << endl;
  HugoTransactions hugoTrans(*ctx->getTab());
  check(hugoTrans.loadTable(ndb, records) == 0, hugoTrans);

  HugoOperations hugoOps(*ctx->getTab());

  /* Check refresh, abort sequence with an ordered index
   * Previously this gave bugs due to corruption of the
   * tuple version
   */
  while (loops--) {
    Uint32 numRefresh = 2 + rand() % 10;

    g_err << "Refresh, rollback * " << numRefresh << endl;

    while (--numRefresh) {
      /* Refresh, rollback */
      check(hugoOps.startTransaction(ndb) == 0, hugoOps);
      check(hugoOps.pkRefreshRecord(ndb, 0, records, 0) == 0, hugoOps);
      check(hugoOps.execute_NoCommit(ndb) == 0, hugoOps);
      check(hugoOps.execute_Rollback(ndb) == 0, hugoOps);
      check(hugoOps.closeTransaction(ndb) == 0, hugoOps);
    }

    g_err << "Refresh, commit" << endl;
    /* Refresh, commit */
    check(hugoOps.startTransaction(ndb) == 0, hugoOps);
    check(hugoOps.pkRefreshRecord(ndb, 0, records, 0) == 0, hugoOps);
    check(hugoOps.execute_NoCommit(ndb) == 0, hugoOps);
    check(hugoOps.execute_Commit(ndb) == 0, hugoOps);
    check(hugoOps.closeTransaction(ndb) == 0, hugoOps);

    g_err << "Update, commit" << endl;
    /* Update */
    check(hugoOps.startTransaction(ndb) == 0, hugoOps);
    check(hugoOps.pkUpdateRecord(ndb, 0, records, 2 + loops) == 0, hugoOps);
    check(hugoOps.execute_NoCommit(ndb) == 0, hugoOps);
    check(hugoOps.execute_Commit(ndb) == 0, hugoOps);
    check(hugoOps.closeTransaction(ndb) == 0, hugoOps);
  }

  return NDBT_OK;
}

int runBuildDuring(NDBT_Context *ctx, NDBT_Step *step) {
  // Verify that data in index match
  // table data
  const int Threads = ctx->getProperty("Threads", (Uint32)0);
  const int loops = ctx->getNumLoops();

  for (int i = 0; i < loops; i++) {
#if 1
    if (createPkIndex(ctx, step) != NDBT_OK) {
      g_err << "Failed to create index" << endl;
      return NDBT_FAILED;
    }
#endif

    if (ctx->isTestStopped()) break;

#if 1
    if (createRandomIndex(ctx, step) != NDBT_OK) {
      g_err << "Failed to create index" << endl;
      return NDBT_FAILED;
    }
#endif

    if (ctx->isTestStopped()) break;

    if (Threads) {
      ctx->setProperty("pause", 1);
      int count = 0;
      for (int j = 0; count < Threads && !ctx->isTestStopped();
           j = (j + 1) % Threads) {
        char buf[255];
        sprintf(buf, "Thread%d_paused", j);
        int tmp = ctx->getProperty(buf, (Uint32)0);
        count += tmp;
      }
    }

    if (ctx->isTestStopped()) break;

#if 1
    if (createPkIndex_Drop(ctx, step) != NDBT_OK) {
      g_err << "Failed to drop index" << endl;
      return NDBT_FAILED;
    }
#endif

    if (ctx->isTestStopped()) break;

#if 1
    if (createRandomIndex_Drop(ctx, step) != NDBT_OK) {
      g_err << "Failed to drop index" << endl;
      return NDBT_FAILED;
    }
#endif

    if (Threads) {
      ctx->setProperty("pause", (Uint32)0);
      NdbSleep_SecSleep(2);
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

static NdbLockable g_lock;
static int threadCounter = 0;

void wait_paused(NDBT_Context *ctx, int id) {
  if (ctx->getProperty("pause", (Uint32)0) == 1) {
    char buf[255];
    sprintf(buf, "Thread%d_paused", id);
    ctx->setProperty(buf, 1);
    while (!ctx->isTestStopped() && ctx->getProperty("pause", (Uint32)0) == 1) {
      NdbSleep_MilliSleep(250);
    }
    ctx->setProperty(buf, (Uint32)0);
  }
}

int runTransactions4(NDBT_Context *ctx, NDBT_Step *step) {
  g_lock.lock();
  const int ThreadId = threadCounter++;
  g_lock.unlock();

  // Verify that data in index match
  // table data
  Ndb *pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 32);
  const int parallel = batchSize > 240 ? 240 : batchSize;

  int rows = ctx->getNumRecords();
  while (ctx->isTestStopped() == false) {
    if (hugoTrans.loadTable(pNdb, rows, batchSize, false) != 0) {
      g_err << "Load table failed" << endl;
      return NDBT_FAILED;
    }

    wait_paused(ctx, ThreadId);

    if (ctx->isTestStopped()) break;

    if (hugoTrans.pkUpdateRecords(pNdb, rows, batchSize) != 0) {
      g_err << "Updated table failed" << endl;
      return NDBT_FAILED;
    }

    wait_paused(ctx, ThreadId);

    if (ctx->isTestStopped()) break;

    if (hugoTrans.scanUpdateRecords(pNdb, rows, 5, parallel) != 0) {
      g_err << "Scan updated table failed" << endl;
      return NDBT_FAILED;
    }

    wait_paused(ctx, ThreadId);

    if (ctx->isTestStopped()) break;

    if (utilTrans.clearTable(pNdb, rows, parallel) != 0) {
      g_err << "Clear table failed" << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int runUniqueNullTransactions(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);

  bool logged = ctx->getProperty("LoggedIndexes", 1);
  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);
  NdbConnection *pTrans = 0;

  const NdbDictionary::Table *pTab = ctx->getTab();
  // Create index
  char nullIndex[255];
  BaseString::snprintf(nullIndex, 255, "IDC_PK_%s_NULL", pTab->getName());
  if (orderedIndex)
    ndbout << "Creating " << ((logged) ? "logged " : "temporary ")
           << "ordered index " << pkIdxName << " (";
  else
    ndbout << "Creating " << ((logged) ? "logged " : "temporary ")
           << "unique index " << pkIdxName << " (";

  NdbDictionary::Index pIdx(pkIdxName);
  pIdx.setTable(pTab->getName());
  if (orderedIndex)
    pIdx.setType(NdbDictionary::Index::OrderedIndex);
  else
    pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
  pIdx.setStoredIndex(logged);
  int c;
  for (c = 0; c < pTab->getNoOfColumns(); c++) {
    const NdbDictionary::Column *col = pTab->getColumn(c);
    if (col->getPrimaryKey()) {
      pIdx.addIndexColumn(col->getName());
      ndbout << col->getName() << " ";
    }
  }

  int colId = -1;
  for (c = 0; c < pTab->getNoOfColumns(); c++) {
    const NdbDictionary::Column *col = pTab->getColumn(c);
    if (col->getNullable()) {
      pIdx.addIndexColumn(col->getName());
      ndbout << col->getName() << " ";
      colId = c;
      break;
    }
  }
  ndbout << ") ";

  if (colId == -1) {
    ndbout << endl << "No nullable column found -> NDBT_FAILED" << endl;
    return NDBT_FAILED;
  }

  bool noddl = ctx->getProperty("NoDDL");
  if (noddl) {
    const NdbDictionary::Index *idx =
        pNdb->getDictionary()->getIndex(pIdx.getName(), pTab->getName());

    if (!idx) {
      ndbout << "Failed - Index does not exist and DDL not allowed" << endl;
      NDB_ERR(pNdb->getDictionary()->getNdbError());
      return NDBT_FAILED;
    } else {
      // TODO : Check index definition is ok
    }
  } else {
    if (pNdb->getDictionary()->createIndex(pIdx) != 0) {
      ndbout << "FAILED!" << endl;
      const NdbError err = pNdb->getDictionary()->getNdbError();
      NDB_ERR(err);
      return NDBT_FAILED;
    }
  }

  int result = NDBT_OK;

  HugoTransactions hugoTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 50);
  int loops = ctx->getNumLoops();
  int rows = ctx->getNumRecords();
  while (loops-- > 0 && ctx->isTestStopped() == false) {
    if (hugoTrans.pkUpdateRecords(pNdb, rows, batchSize) != 0) {
      g_err << "Updated table failed" << endl;
      result = NDBT_FAILED;
      goto done;
    }
  }

  if (ctx->isTestStopped()) {
    goto done;
  }

  ctx->stopTest();
  while (ctx->getNoOfRunningSteps() > 1) {
    NdbSleep_MilliSleep(100);
  }

  result = NDBT_FAILED;
  pTrans = pNdb->startTransaction();
  NdbScanOperation *sOp;

  int eof;
  if (!pTrans) goto done;
  sOp = pTrans->getNdbScanOperation(pTab->getName());
  if (!sOp) goto done;
  if (sOp->readTuples(NdbScanOperation::LM_Exclusive)) goto done;
  if (pTrans->execute(NoCommit) == -1) goto done;
  while ((eof = sOp->nextResult(true)) == 0) {
    do {
      NdbOperation *uOp = sOp->updateCurrentTuple();
      if (uOp == 0) goto done;
      uOp->setValue(colId, 0);
    } while ((eof = sOp->nextResult(false)) == 0);
    eof = pTrans->execute(Commit);
    if (eof == -1) goto done;
  }

done:
  if (pTrans) pNdb->closeTransaction(pTrans);
  pNdb->getDictionary()->dropIndex(nullIndex, pTab->getName());
  return result;
}

int runLQHKEYREF(NDBT_Context *ctx, NDBT_Step *step) {
  int loops = ctx->getNumLoops() * 100;
  NdbRestarter restarter;

  myRandom48Init((long)NdbTick_CurrentMillisecond());

#if 0
  int val = DumpStateOrd::DihMinTimeBetweenLCP;
  if(restarter.dumpStateAllNodes(&val, 1) != 0){
    g_err << "Failed to dump DihMinTimeBetweenLCP" << endl;
    return NDBT_FAILED;
  }
#endif

  for (int i = 0; i < loops && !ctx->isTestStopped(); i++) {
    int randomId = myRandom48(restarter.getNumDbNodes());
    int nodeId = restarter.getDbNodeId(randomId);

    const Uint32 error = 5031 + (i % 3);

    if (restarter.insertErrorInNode(nodeId, error) != 0) {
      g_err << "Failed to error insert( " << error << ") in node " << nodeId
            << endl;
      return NDBT_FAILED;
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runBug21384(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  NdbRestarter restarter;

  int loops = ctx->getNumLoops();
  const int rows = ctx->getNumRecords();
  const int batchsize = ctx->getProperty("BatchSize", 50);

  while (loops--) {
    if (restarter.insertErrorInAllNodes(8037) != 0) {
      g_err << "Failed to error insert(8037)" << endl;
      return NDBT_FAILED;
    }

    if (hugoTrans.indexReadRecords(pNdb, pkIdxName, rows, batchsize) == 0) {
      g_err << "Index succeded (it should have failed" << endl;
      return NDBT_FAILED;
    }

    if (restarter.insertErrorInAllNodes(0) != 0) {
      g_err << "Failed to error insert(0)" << endl;
      return NDBT_FAILED;
    }

    if (hugoTrans.indexReadRecords(pNdb, pkIdxName, rows, batchsize) != 0) {
      g_err << "Index read failed" << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runReadIndexUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  int rows = ctx->getNumRecords();
  while (!ctx->isTestStopped()) {
    hugoTrans.indexReadRecords(pNdb, pkIdxName, rows, 1);
  }
  return NDBT_OK;
}

int runBug25059(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();
  const NdbDictionary::Index *idx =
      dict->getIndex(pkIdxName, ctx->getTab()->getName());

  HugoOperations ops(*ctx->getTab(), idx);

  int res = NDBT_OK;
  int loops = ctx->getNumLoops();
  const int rows = ctx->getNumRecords();

  while (res == NDBT_OK && loops--) {
    ops.startTransaction(pNdb);
    ops.pkReadRecord(pNdb, 10 + rand() % rows, rows);
    int tmp;
    if ((tmp = ops.execute_Commit(pNdb, AO_IgnoreError))) {
      if (tmp == 4012)
        res = NDBT_FAILED;
      else if (ops.getTransaction()->getNdbError().code == 4012)
        res = NDBT_FAILED;
    }
    ops.closeTransaction(pNdb);
  }

  loops = ctx->getNumLoops();
  while (res == NDBT_OK && loops--) {
    ops.startTransaction(pNdb);
    ops.pkUpdateRecord(pNdb, 10 + rand() % rows, rows);
    int tmp;
    int arg = 0;
    switch (rand() % 2) {
      case 0:
        arg = AbortOnError;
        break;
      case 1:
        arg = AO_IgnoreError;
        ndbout_c("ignore error");
        break;
    }
    if ((tmp = ops.execute_Commit(pNdb, (AbortOption)arg))) {
      if (tmp == 4012)
        res = NDBT_FAILED;
      else if (ops.getTransaction()->getNdbError().code == 4012)
        res = NDBT_FAILED;
    }
    ops.closeTransaction(pNdb);
  }

  return res;
}

// From 6.3.X, Unique index operations do not use
// TransactionBufferMemory.
// Long signal KeyInfo and AttrInfo storage exhaustion
// is already tested by testLimits
// Testing of segment exhaustion when accumulating from
// signal trains cannot be tested from 7.0 as we cannot
// generate short signal trains.
// TODO : Execute testcase as part of upgrade testing -
// 6.3 to 7.0?
int tcSaveINDX_test(NDBT_Context *ctx, NDBT_Step *step, int inject_err) {
  int result = NDBT_OK;
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();
  const NdbDictionary::Index *idx =
      dict->getIndex(pkIdxName, ctx->getTab()->getName());

  HugoOperations ops(*ctx->getTab(), idx);

  g_err << "Using INDEX: " << pkIdxName << endl;

  NdbRestarter restarter;

  int loops = ctx->getNumLoops();
  const int rows = ctx->getNumRecords();

  for (int bs = 1; bs < loops; bs++) {
    int c = 0;
    while (c++ < loops) {
      g_err << "BS " << bs << " LOOP #" << c << endl;

      g_err << "inserting error on op#" << c << endl;

      CHECK(ops.startTransaction(pNdb) == 0);
      for (int i = 1; i <= c; i++) {
        if (i == c) {
          if (restarter.insertErrorInAllNodes(inject_err) != 0) {
            g_err << "**** FAILED to insert error" << endl;
            result = NDBT_FAILED;
            break;
          }
        }
        CHECK(ops.indexReadRecords(pNdb, pkIdxName, i, false, 1) == 0);
        if (i % bs == 0 || i == c) {
          if (i < c) {
            if (ops.execute_NoCommit(pNdb, AO_IgnoreError) != NDBT_OK) {
              g_err << "**** executeNoCommit should have succeeded" << endl;
              result = NDBT_FAILED;
            }
          } else {
            if (ops.execute_NoCommit(pNdb, AO_IgnoreError) != 289) {
              g_err << "**** executeNoCommit should have failed with 289"
                    << endl;
              result = NDBT_FAILED;
            }
            g_err << "NdbError.code= "
                  << ops.getTransaction()->getNdbError().code << endl;
            break;
          }
        }
      }

      CHECK(ops.closeTransaction(pNdb) == 0);

      if (restarter.insertErrorInAllNodes(0) != 0) {
        g_err << "**** Failed to error insert(0)" << endl;
        return NDBT_FAILED;
      }

      CHECK(ops.startTransaction(pNdb) == 0);
      if (ops.indexReadRecords(pNdb, pkIdxName, 0, 0, rows) != 0) {
        g_err << "**** Index read failed" << endl;
        return NDBT_FAILED;
      }
      CHECK(ops.closeTransaction(pNdb) == 0);
    }
  }

  return result;
}

int runBug28804(NDBT_Context *ctx, NDBT_Step *step) {
  return tcSaveINDX_test(ctx, step, 8052);
}

int runBug28804_ATTRINFO(NDBT_Context *ctx, NDBT_Step *step) {
  return tcSaveINDX_test(ctx, step, 8051);
}

int runBug46069(NDBT_Context *ctx, NDBT_Step *step) {
  HugoTransactions hugoTrans(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);
  const int rows = ctx->getNumRecords();
  Uint32 threads = ctx->getProperty("THREADS", 12);
  int loops = ctx->getNumLoops();

  ctx->getPropertyWait("STARTED", threads);

  for (int i = 0; i < loops; i++) {
    ndbout << "Loop: " << i << endl;
    if (hugoTrans.loadTable(pNdb, rows) != 0) return NDBT_FAILED;

    ctx->setProperty("STARTED", Uint32(0));
    ctx->getPropertyWait("STARTED", threads);
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runBug46069_pkdel(NDBT_Context *ctx, NDBT_Step *step) {
  HugoOperations hugoOps(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);
  const int rows = ctx->getNumRecords();

  while (!ctx->isTestStopped()) {
    ctx->incProperty("STARTED");
    ctx->getPropertyWait("STARTED", Uint32(0));
    if (ctx->isTestStopped()) break;

    for (int i = 0; i < rows && !ctx->isTestStopped();) {
      int cnt = (rows - i);
      if (cnt > 100) cnt = 100;
      cnt = 1 + (rand() % cnt);
      if (hugoOps.startTransaction(pNdb) != 0) {
        break;
      }
      hugoOps.pkDeleteRecord(pNdb, i, cnt);
      int res = hugoOps.execute_Commit(pNdb, AO_IgnoreError);
      if (res != -1) {
        i += cnt;
      }
      hugoOps.closeTransaction(pNdb);
    }
  }

  return NDBT_OK;
}

int runBug46069_scandel(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();
  const NdbDictionary::Index *idx =
      dict->getIndex(pkIdxName, ctx->getTab()->getName());
  if (idx == 0) {
    return NDBT_FAILED;
  }
  UtilTransactions hugoTrans(*ctx->getTab(), idx);

  while (!ctx->isTestStopped()) {
    ctx->incProperty("STARTED");
    ctx->getPropertyWait("STARTED", Uint32(0));
    if (ctx->isTestStopped()) break;

    hugoTrans.clearTable(pNdb);
  }

  return NDBT_OK;
}

int runBug50118(NDBT_Context *ctx, NDBT_Step *step) {
  NdbSleep_MilliSleep(500);
  int loops = ctx->getNumLoops();
  while (loops--) {
    createPkIndex_Drop(ctx, step);
    createPkIndex(ctx, step);
  }
  ctx->stopTest();
  return NDBT_OK;
}

int runTrigOverload(NDBT_Context *ctx, NDBT_Step *step) {
  /* Test inserts, deletes and updates via
   * PK with error inserts
   */
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  NdbRestarter restarter;

  unsigned numScenarios = 3;
  unsigned errorInserts[3] = {8085, 8086, 0};
  int results[3] = {293,  // Inconsistent trigger state in TC block
                    218,  // Out of LongMessageBuffer
                    0};

  unsigned iterations = 50;

  /* Insert some records */
  if (hugoOps.startTransaction(pNdb) ||
      hugoOps.pkInsertRecord(pNdb, 0, iterations) ||
      hugoOps.execute_Commit(pNdb)) {
    g_err << "Failed on initial insert " << pNdb->getNdbError() << endl;
    return NDBT_FAILED;
  }

  hugoOps.closeTransaction(pNdb);

  for (unsigned i = 0; i < iterations; i++) {
    unsigned scenario = i % numScenarios;
    unsigned errorVal = errorInserts[scenario];
    g_err << "Iteration :" << i << " inserting error " << errorVal
          << " expecting result : " << results[scenario] << endl;
    restarter.insertErrorInAllNodes(errorVal);
    //    NdbSleep_MilliSleep(500); // Error insert latency?

    CHECKRET(hugoOps.startTransaction(pNdb) == 0);

    CHECKRET(hugoOps.pkInsertRecord(pNdb, iterations + i, 1) == 0);

    hugoOps.execute_Commit(pNdb);

    int errorCode = hugoOps.getTransaction()->getNdbError().code;

    if (errorCode != results[scenario]) {
      g_err << "For Insert in scenario " << scenario << " expected code "
            << results[scenario] << " but got "
            << hugoOps.getTransaction()->getNdbError() << endl;
      return NDBT_FAILED;
    }

    hugoOps.closeTransaction(pNdb);

    CHECKRET(hugoOps.startTransaction(pNdb) == 0);

    CHECKRET(hugoOps.pkUpdateRecord(pNdb, i, 1, iterations) == 0);

    hugoOps.execute_Commit(pNdb);

    errorCode = hugoOps.getTransaction()->getNdbError().code;

    if (errorCode != results[scenario]) {
      g_err << "For Update in scenario " << scenario << " expected code "
            << results[scenario] << " but got "
            << hugoOps.getTransaction()->getNdbError() << endl;
      return NDBT_FAILED;
    }

    hugoOps.closeTransaction(pNdb);

    CHECKRET(hugoOps.startTransaction(pNdb) == 0);

    CHECKRET(hugoOps.pkDeleteRecord(pNdb, i, 1) == 0);

    hugoOps.execute_Commit(pNdb);

    errorCode = hugoOps.getTransaction()->getNdbError().code;

    if (errorCode != results[scenario]) {
      g_err << "For Delete in scenario " << scenario << " expected code "
            << results[scenario] << " but got "
            << hugoOps.getTransaction()->getNdbError() << endl;
      return NDBT_FAILED;
    }

    hugoOps.closeTransaction(pNdb);
  }

  restarter.insertErrorInAllNodes(0);

  return NDBT_OK;
}

int runClearError(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;

  restarter.insertErrorInAllNodes(0);

  return NDBT_OK;
}

// bug#56829

#undef CHECK2  // previous no good
#define CHECK2(b, e)                                                      \
  if (!(b)) {                                                             \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ << ": " << e \
          << endl;                                                        \
    result = NDBT_FAILED;                                                 \
    break;                                                                \
  }

static int get_data_memory_pages(NdbMgmHandle h, NdbNodeBitmask dbmask,
                                 int *pages_out) {
  int result = NDBT_OK;
  int pages = 0;

  while (1) {
    // sends dump 1000 and retrieves all replies
    ndb_mgm_events *e = 0;
    CHECK2((e = ndb_mgm_dump_events(h, NDB_LE_MemoryUsage, 0, 0)) != 0,
           ndb_mgm_get_latest_error_msg(h));

    // sum up pages (also verify sanity)
    for (int i = 0; i < e->no_of_events; i++) {
      ndb_logevent *le = &e->events[i];
      CHECK2(le->type == NDB_LE_MemoryUsage, "bad event type " << le->type);
      const ndb_logevent_MemoryUsage *lem = &le->MemoryUsage;
      if (lem->block != DBTUP) continue;
      int nodeId = le->source_nodeid;
      CHECK2(dbmask.get(nodeId), "duplicate event from node " << nodeId);
      dbmask.clear(nodeId);
      pages += lem->pages_used;
      g_info << "i:" << i << " node:" << le->source_nodeid
             << " pages:" << lem->pages_used << endl;
    }
    free(e);
    CHECK2(result == NDBT_OK, "failed");

    char buf[NdbNodeBitmask::TextLength + 1];
    CHECK2(dbmask.isclear(), "no response from nodes " << dbmask.getText(buf));
    break;
  }

  *pages_out = pages;
  return result;
}

int runBug56829(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDic = pNdb->getDictionary();
  const int loops = ctx->getNumLoops();
  int result = NDBT_OK;
  const NdbDictionary::Table tab(*ctx->getTab());
  const int rows = ctx->getNumRecords();
  const char *mgm = 0;  // XXX ctx->getRemoteMgm();

  TlsKeyManager tlsKeyManager;
  tlsKeyManager.init_mgm_client(opt_tls_search_path);

  char tabname[100];
  strcpy(tabname, tab.getName());
  char indname[100];
  strcpy(indname, tabname);
  strcat(indname, "X1");

  (void)pDic->dropTable(tabname);

  NdbMgmHandle h = 0;
  NdbNodeBitmask dbmask;
  // entry n marks if row with PK n exists
  char *rowmask = new char[rows];
  std::memset(rowmask, 0, rows);
  int loop = 0;
  while (loop < loops) {
    CHECK2(rows > 0, "rows must be != 0");
    g_err << "loop " << loop << "<" << loops << endl;

    // at first loop connect to mgm
    if (loop == 0) {
      CHECK2((h = ndb_mgm_create_handle()) != 0,
             "mgm: failed to create handle");
      CHECK2(ndb_mgm_set_connectstring(h, mgm) == 0,
             ndb_mgm_get_latest_error_msg(h));
      ndb_mgm_set_ssl_ctx(h, tlsKeyManager.ctx());
      CHECK2(ndb_mgm_connect_tls(h, 0, 0, 0, opt_mgm_tls) == 0,
             ndb_mgm_get_latest_error_msg(h));
      g_info << "mgm: connected to " << (mgm ? mgm : "default") << endl;

      // make bitmask of DB nodes
      dbmask.clear();
      ndb_mgm_cluster_state *cs = 0;
      CHECK2((cs = ndb_mgm_get_status(h)) != 0,
             ndb_mgm_get_latest_error_msg(h));
      for (int j = 0; j < cs->no_of_nodes; j++) {
        ndb_mgm_node_state *ns = &cs->node_states[j];
        if (ns->node_type == NDB_MGM_NODE_TYPE_NDB) {
          CHECK2(ns->node_status == NDB_MGM_NODE_STATUS_STARTED,
                 "node " << ns->node_id << " not started status "
                         << ns->node_status);
          CHECK2(!dbmask.get(ns->node_id), "duplicate node id " << ns->node_id);
          dbmask.set(ns->node_id);
          g_info << "added DB node " << ns->node_id << endl;
        }
      }
      free(cs);
      CHECK2(result == NDBT_OK, "some DB nodes are not started");
      CHECK2(!dbmask.isclear(), "found no DB nodes");
    }

    // data memory pages after following events
    // 0-initial 1,2-create table,index 3-load 4-delete 5,6-drop index,table
    int pages[7];

    // initial
    CHECK2(get_data_memory_pages(h, dbmask, &pages[0]) == NDBT_OK, "failed");
    g_err << "initial pages " << pages[0] << endl;

    // create table
    g_err << "create table " << tabname << endl;
    const NdbDictionary::Table *pTab = 0;
    CHECK2(pDic->createTable(tab) == 0, pDic->getNdbError());
    CHECK2((pTab = pDic->getTable(tabname)) != 0, pDic->getNdbError());
    CHECK2(get_data_memory_pages(h, dbmask, &pages[1]) == NDBT_OK, "failed");
    g_err << "create table pages " << pages[1] << endl;

    // choice of index attributes is not relevant to this bug
    // choose one non-PK updateable column
    NdbDictionary::Index ind;
    ind.setName(indname);
    ind.setTable(tabname);
    ind.setType(NdbDictionary::Index::OrderedIndex);
    ind.setLogging(false);
    {
      HugoCalculator calc(*pTab);
      for (int j = 0; j < pTab->getNoOfColumns(); j++) {
        const NdbDictionary::Column *col = pTab->getColumn(j);
        if (col->getPrimaryKey() || calc.isUpdateCol(j)) continue;
        // CHARSET_INFO* cs = col->getCharset();
        if (NdbSqlUtil::check_column_for_ordered_index(
                col->getType(), col->getCharset()) == 0) {
          ind.addColumn(*col);
          break;
        }
      }
    }
    CHECK2(ind.getNoOfColumns() == 1, "cannot use table " << tabname);

    // create index
    g_err << "create index " << indname << " on " << ind.getColumn(0)->getName()
          << endl;
    const NdbDictionary::Index *pInd = 0;
    CHECK2(pDic->createIndex(ind, *pTab) == 0, pDic->getNdbError());
    CHECK2((pInd = pDic->getIndex(indname, tabname)) != 0, pDic->getNdbError());
    CHECK2(get_data_memory_pages(h, dbmask, &pages[2]) == NDBT_OK, "failed");
    g_err << "create index pages " << pages[2] << endl;

    HugoTransactions trans(*pTab);

    // load all records
    g_err << "load records" << endl;
    CHECK2(trans.loadTable(pNdb, rows) == 0, trans.getNdbError());
    std::memset(rowmask, 1, rows);
    CHECK2(get_data_memory_pages(h, dbmask, &pages[3]) == NDBT_OK, "failed");
    g_err << "load records pages " << pages[3] << endl;

    // test index with random ops
    g_info << "test index ops" << endl;
    {
      HugoOperations ops(*pTab);
      for (int i = 0; i < rows; i++) {
        CHECK2(ops.startTransaction(pNdb) == 0, ops.getNdbError());
        for (int j = 0; j < 32; j++) {
          int n = rand() % rows;
          if (!rowmask[n]) {
            CHECK2(ops.pkInsertRecord(pNdb, n) == 0, ops.getNdbError());
            rowmask[n] = 1;
          } else if (rand() % 2 == 0) {
            CHECK2(ops.pkDeleteRecord(pNdb, n) == 0, ops.getNdbError());
            rowmask[n] = 0;
          } else {
            CHECK2(ops.pkUpdateRecord(pNdb, n) == 0, ops.getNdbError());
          }
        }
        CHECK2(result == NDBT_OK, "index ops batch failed");
        CHECK2(ops.execute_Commit(pNdb) == 0, ops.getNdbError());
        ops.closeTransaction(pNdb);
      }
      CHECK2(result == NDBT_OK, "index ops failed");
    }

    // delete all records
    g_err << "delete records" << endl;
    CHECK2(trans.clearTable(pNdb) == 0, trans.getNdbError());
    memset(rowmask, 0, rows);
    NdbSleep_SecSleep(2);
    CHECK2(get_data_memory_pages(h, dbmask, &pages[4]) == NDBT_OK, "failed");
    g_err << "delete records pages " << pages[4] << endl;

    // drop index
    g_err << "drop index" << endl;
    CHECK2(pDic->dropIndex(indname, tabname) == 0, pDic->getNdbError());
    CHECK2(get_data_memory_pages(h, dbmask, &pages[5]) == NDBT_OK, "failed");
    g_err << "drop index pages " << pages[5] << endl;

    // drop table
    g_err << "drop table" << endl;
    CHECK2(pDic->dropTable(tabname) == 0, pDic->getNdbError());
    CHECK2(get_data_memory_pages(h, dbmask, &pages[6]) == NDBT_OK, "failed");
    g_err << "drop table pages " << pages[6] << endl;

    // verify
    /**
     * Even after dropping all rows, we might still have data memory pages
     * allocated for fragment page maps. So only after dropping both index
     * and tables can we rely on all memory allocated for a table to be
     * dropped. But we can assume that create table will not allocate any pages.
     * Create index on the other hand will allocate pages for auto index stats.
     */
    CHECK2(pages[1] == pages[0], "pages after create table "
                                     << pages[1] << " not == initial pages "
                                     << pages[0]);
    CHECK2(pages[2] > pages[0], "pages after create index "
                                    << pages[2] << " not > initial pages "
                                    << pages[0]);
    CHECK2(pages[3] > pages[0], "pages after load " << pages[3]
                                                    << " not >  initial pages "
                                                    << pages[0]);
    CHECK2(pages[4] < pages[3], "pages after delete "
                                    << pages[4] << " not == initial pages "
                                    << pages[0]);
    CHECK2(pages[5] < pages[3], "pages after drop index "
                                    << pages[5] << " not == initial pages "
                                    << pages[0]);
    CHECK2(pages[6] == pages[0], "pages after drop table "
                                     << pages[6] << " not == initial pages "
                                     << pages[0]);

    loop++;

    // at last loop disconnect from mgm
    if (loop == loops) {
      CHECK2(ndb_mgm_disconnect(h) == 0, ndb_mgm_get_latest_error_msg(h));
      ndb_mgm_destroy_handle(&h);
      g_info << "mgm: disconnected" << endl;
    }
  }
  delete[] rowmask;

  return result;
}

#define CHK_RET_FAILED(x)                     \
  if (!(x)) {                                 \
    ndbout_c("Failed on line: %u", __LINE__); \
    return NDBT_FAILED;                       \
  }

int runBug12315582(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();

  const NdbDictionary::Index *pIdx = dict->getIndex(pkIdxName, pTab->getName());
  CHK_RET_FAILED(pIdx != 0);

  const NdbRecord *pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0);
  const NdbRecord *pIdxRecord = pIdx->getDefaultRecord();
  CHK_RET_FAILED(pIdxRecord != 0);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 *pRow = new Uint8[len];
  std::memset(pRow, 0, len);

  HugoCalculator calc(*pTab);
  calc.equalForRow(pRow, pRowRecord, 0);

  NdbTransaction *pTrans = pNdb->startTransaction();
  CHK_RET_FAILED(pTrans != 0);

  const NdbOperation *pOp[2] = {0, 0};
  for (Uint32 i = 0; i < 2; i++) {
    NdbInterpretedCode code;
    if (i == 0)
      code.interpret_exit_ok();
    else
      code.interpret_exit_nok();

    code.finalise();

    NdbOperation::OperationOptions opts;
    std::memset(&opts, 0, sizeof(opts));
    opts.optionsPresent = NdbOperation::OperationOptions::OO_INTERPRETED;
    opts.interpretedCode = &code;

    pOp[i] =
        pTrans->readTuple(pIdxRecord, (char *)pRow, pRowRecord, (char *)pRow,
                          NdbOperation::LM_Read, 0, &opts, sizeof(opts));
    CHK_RET_FAILED(pOp[i]);
  }

  int res = pTrans->execute(Commit, AO_IgnoreError);

  CHK_RET_FAILED(res == 0);
  CHK_RET_FAILED(pOp[0]->getNdbError().code == 0);
  CHK_RET_FAILED(pOp[1]->getNdbError().code != 0);

  delete[] pRow;

  return NDBT_OK;
}

int runBug60851(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();

  const NdbDictionary::Index *pIdx = dict->getIndex(pkIdxName, pTab->getName());
  CHK_RET_FAILED(pIdx != 0);

  const NdbRecord *pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0);
  const NdbRecord *pIdxRecord = pIdx->getDefaultRecord();
  CHK_RET_FAILED(pIdxRecord != 0);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 *pRow = new Uint8[len];

  NdbTransaction *pTrans = pNdb->startTransaction();
  CHK_RET_FAILED(pTrans != 0);

  const NdbOperation *pOp[3] = {0, 0, 0};
  for (Uint32 i = 0; i < 3; i++) {
    NdbInterpretedCode code;
    if (i == 1)
      code.interpret_exit_nok();
    else
      code.interpret_exit_ok();

    code.finalise();

    std::memset(pRow, 0, len);
    HugoCalculator calc(*pTab);
    calc.equalForRow(pRow, pRowRecord, i);

    NdbOperation::OperationOptions opts;
    std::memset(&opts, 0, sizeof(opts));
    opts.optionsPresent = NdbOperation::OperationOptions::OO_INTERPRETED;
    opts.interpretedCode = &code;

    pOp[i] = pTrans->deleteTuple(pIdxRecord, (char *)pRow, pRowRecord,
                                 (char *)pRow, 0, &opts, sizeof(opts));
    CHK_RET_FAILED(pOp[i]);
  }

  int res = pTrans->execute(Commit, AO_IgnoreError);

  CHK_RET_FAILED(res == 0);
  CHK_RET_FAILED(pOp[0]->getNdbError().code == 0);
  CHK_RET_FAILED(pOp[1]->getNdbError().code != 0);
  CHK_RET_FAILED(pOp[2]->getNdbError().code == 0);

  delete[] pRow;

  return NDBT_OK;
}

static const int deferred_errors[] = {
    5064, 0, 5065, 0, 5066, 0, 5067, 0, 5068, 0, 5069, 0,
    5070, 0, 5071, 0, 5072, 1, 8090, 0, 8091, 0, 8092, 2,  // connected tc
    0,    0                                                // trailer
};

int runTestDeferredError(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;
  Ndb *pNdb = GETNDB(step);
  const NdbDictionary::Table *pTab = ctx->getTab();

  const int rows = ctx->getNumRecords();

  const NdbRecord *pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 *pRow = new Uint8[len];

  for (int i = 0; deferred_errors[i] != 0; i += 2) {
    const int errorno = deferred_errors[i];
    const int nodefail = deferred_errors[i + 1];

    for (int j = 0; j < 3; j++) {
      NdbTransaction *pTrans = pNdb->startTransaction();
      CHK_RET_FAILED(pTrans != 0);

      int nodeId = nodefail == 0   ? 0
                   : nodefail == 1 ? res.getNode(NdbRestarter::NS_RANDOM)
                   : nodefail == 2 ? pTrans->getConnectedNodeId()
                                   : 0;

      ndbout_c("errorno: %u(nf: %u - %u) j: %u : %s", errorno, nodefail, nodeId,
               j,
               j == 0   ? "test before error insert"
               : j == 1 ? "test with error insert"
               : j == 2 ? "test after error insert"
                        : "");
      if (j == 0 || j == 2) {
        // First time succeed
        // Last time succeed
      } else if (nodefail == 0) {
        CHK_RET_FAILED(res.insertErrorInAllNodes(errorno) == 0);
      } else {
        int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
        CHK_RET_FAILED(res.dumpStateOneNode(nodeId, val2, 2) == 0);
        CHK_RET_FAILED(res.insertErrorInNode(nodeId, errorno) == 0);
      }

      for (int rowNo = 0; rowNo < 100; rowNo++) {
        int rowId = rand() % rows;
        std::memset(pRow, 0, len);

        HugoCalculator calc(*pTab);
        calc.setValues(pRow, pRowRecord, rowId, rand());

        NdbOperation::OperationOptions opts;
        std::memset(&opts, 0, sizeof(opts));
        opts.optionsPresent =
            NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;

        const NdbOperation *pOp =
            pTrans->updateTuple(pRowRecord, (char *)pRow, pRowRecord,
                                (char *)pRow, 0, &opts, sizeof(opts));
        CHK_RET_FAILED(pOp != 0);
      }

      int result = pTrans->execute(Commit, AO_IgnoreError);
      if (j == 0 || j == 2) {
        CHK_RET_FAILED(result == 0);
      } else {
        CHK_RET_FAILED(result != 0);
      }
      pTrans->close();

      if (j == 0 || j == 2) {
      } else {
        if (nodefail) {
          ndbout_c("  waiting for %u to enter not-started", nodeId);
          // Wait for a node to enter not-started
          CHK_RET_FAILED(res.waitNodesNoStart(&nodeId, 1) == 0);

          ndbout_c("  starting all");
          CHK_RET_FAILED(res.startAll() == 0);
          ndbout_c("  wait cluster started");
          CHK_RET_FAILED(res.waitClusterStarted() == 0);
          CHK_NDB_READY(pNdb);
          ndbout_c("  cluster started");
        }
        CHK_RET_FAILED(res.insertErrorInAllNodes(0) == 0);
      }
    }
  }

  delete[] pRow;

  return NDBT_OK;
}

int runMixedDML(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  const NdbDictionary::Table *pTab = ctx->getTab();

  unsigned seed = (unsigned)NdbTick_CurrentMillisecond();

  const int rows = ctx->getNumRecords();
  const int loops = 10 * ctx->getNumLoops();
  const int until_stopped = ctx->getProperty("UntilStopped");
  const int deferred = ctx->getProperty("Deferred");
  const int batch = ctx->getProperty("Batch", Uint32(50));

  const NdbRecord *pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 *pRow = new Uint8[len];

  int count_ok = 0;
  int count_failed = 0;
  for (int i = 0; i < loops || (until_stopped && !ctx->isTestStopped()); i++) {
    NdbTransaction *pTrans = pNdb->startTransaction();
    CHK_RET_FAILED(pTrans != 0);

    int lastrow = 0;
    int result = 0;
    for (int rowNo = 0; rowNo < batch; rowNo++) {
      int left = rows - lastrow;
      int rowId = lastrow;
      if (left) {
        rowId += ndb_rand_r(&seed) % (left / 10 + 1);
      } else {
        break;
      }
      lastrow = rowId;

      std::memset(pRow, 0, len);

      HugoCalculator calc(*pTab);
      calc.setValues(pRow, pRowRecord, rowId, rand());

      NdbOperation::OperationOptions opts;
      std::memset(&opts, 0, sizeof(opts));
      if (deferred) {
        opts.optionsPresent =
            NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
      }

      const NdbOperation *pOp = 0;
      switch (ndb_rand_r(&seed) % 3) {
        case 0:
          pOp = pTrans->writeTuple(pRowRecord, (char *)pRow, pRowRecord,
                                   (char *)pRow, 0, &opts, sizeof(opts));
          break;
        case 1:
          pOp = pTrans->deleteTuple(pRowRecord, (char *)pRow, pRowRecord,
                                    (char *)pRow, 0, &opts, sizeof(opts));
          break;
        case 2:
          pOp = pTrans->updateTuple(pRowRecord, (char *)pRow, pRowRecord,
                                    (char *)pRow, 0, &opts, sizeof(opts));
          break;
      }
      CHK_RET_FAILED(pOp != 0);
      result = pTrans->execute(NoCommit, AO_IgnoreError);
      if (result != 0) {
        goto found_error;
      }
    }

    result = pTrans->execute(Commit, AO_IgnoreError);
    if (result != 0) {
    found_error:
      count_failed++;
      NdbError err = pTrans->getNdbError();
      ndbout << err << endl;
      CHK_RET_FAILED(err.code == 1235 || err.code == 1236 || err.code == 5066 ||
                     err.status == NdbError::TemporaryError ||
                     err.classification == NdbError::NoDataFound ||
                     err.classification == NdbError::ConstraintViolation);
    } else {
      count_ok++;
    }
    pTrans->close();
  }

  ndbout_c("count_ok: %d count_failed: %d", count_ok, count_failed);
  delete[] pRow;

  return NDBT_OK;
}

int runDeferredError(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter res;

  for (int l = 0; l < ctx->getNumLoops() && !ctx->isTestStopped(); l++) {
    for (int i = 0; deferred_errors[i] != 0 && !ctx->isTestStopped(); i += 2) {
      const int errorno = deferred_errors[i];
      const int nodefail = deferred_errors[i + 1];

      int nodeId = res.getNode(NdbRestarter::NS_RANDOM);

      ndbout_c("errorno: %u (nf: %u - %u)", errorno, nodefail, nodeId);

      if (nodefail == 0) {
        CHK_RET_FAILED(res.insertErrorInNode(nodeId, errorno) == 0);
        NdbSleep_MilliSleep(300);
        CHK_RET_FAILED(res.insertErrorInNode(nodeId, 0) == 0);
      } else {
        int val2[] = {DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1};
        CHK_RET_FAILED(res.dumpStateOneNode(nodeId, val2, 2) == 0);
        CHK_RET_FAILED(res.insertErrorInNode(nodeId, errorno) == 0);
        ndbout_c("  waiting for %u to enter not-started", nodeId);
        // Wait for a node to enter not-started
        CHK_RET_FAILED(res.waitNodesNoStart(&nodeId, 1) == 0);

        ndbout_c("  starting all");
        CHK_RET_FAILED(res.startAll() == 0);
        ndbout_c("  wait cluster started");
        CHK_RET_FAILED(res.waitClusterStarted() == 0);
        ndbout_c("  cluster started");
      }
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int runChunkyUpdatesUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * Run 'chunky' UPDATES
   */
  /* Updates run on defined records
   * Some percentage of the defined records are updated in
   * one transaction.
   */
  const Uint32 numRecords = ctx->getNumRecords();
  const Uint32 pctChunk = ctx->getProperty("ChunkPercent", 50);

  Uint32 chunkSize = (numRecords * pctChunk) / 100;
  if (chunkSize == 0) {
    chunkSize = 1;
  };

  HugoOperations hugoOps(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);

  g_err << "Running updates of chunk pct " << pctChunk << " size " << chunkSize
        << " rows until stopped." << endl;

  Uint32 pos = 0;
  Uint32 i = 0;
  while (!ctx->isTestStopped()) {
    CHECKRET(hugoOps.startTransaction(pNdb) == 0);
    for (Uint32 op = 0; op < chunkSize; op++) {
      CHECKRET(hugoOps.pkUpdateRecord(pNdb, pos, 1, (i * numRecords)) == 0);
      pos = (pos + 1) % numRecords;
    }
    CHECKRET(hugoOps.execute_Commit(pNdb) == 0);
    CHECKRET(hugoOps.closeTransaction(pNdb) == 0);
    i++;
  }

  return NDBT_OK;
}

int runChunkyInsertDeletesUntilStopped(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * Run 'chunky' INSERT+DELETE
   */
  /* Run on undefined part of row space
   */
  const Uint32 numRecords = ctx->getNumRecords();
  const Uint32 pctChunk = ctx->getProperty("ChunkPercent", 50);

  Uint32 chunkSize = (numRecords * pctChunk) / 100;
  if (chunkSize == 0) {
    chunkSize = 1;
  };

  g_err << "Running insert/deletes of chunk pct " << pctChunk << " size "
        << chunkSize << " rows until stopped." << endl;

  HugoOperations hugoOps(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);

  Uint32 i = 0;
  Uint32 pos = 0;
  bool insert = true;
  while (!ctx->isTestStopped()) {
    CHECKRET(hugoOps.startTransaction(pNdb) == 0);
    for (Uint32 op = 0; op < chunkSize; op++) {
      if (insert) {
        CHECKRET(hugoOps.pkInsertRecord(pNdb, numRecords + pos, 1,
                                        (i * numRecords)) == 0);
      } else {
        CHECKRET(hugoOps.pkDeleteRecord(pNdb, numRecords + pos) == 0);
      }
      pos++;
      if (pos == numRecords) {
        insert = !insert;
        pos = 0;
      }
    }
    CHECKRET(hugoOps.execute_Commit(pNdb) == 0);
    CHECKRET(hugoOps.closeTransaction(pNdb) == 0);
    i++;
  }

  return NDBT_OK;
}

int runRandomIndexScan(NDBT_Context *ctx, NDBT_Step *step) {
  /**
   * Run a series of CommittedRead scans using the
   * randomly created index.
   * No attention is paid to the results returned.
   * Batchsize is user defined.
   */
  const Uint32 idx = ctx->getProperty("createRandomIndex");
  char iName[20];
  BaseString::snprintf(iName, 20, "IDC%d", idx);

  const Uint32 scanBatchSize = ctx->getProperty("scanBatchSize", Uint32(0));

  Ndb *pNdb = GETNDB(step);
  const NdbDictionary::Index *pRandomIndex =
      pNdb->getDictionary()->getIndex(iName, ctx->getTab()->getName());
  CHECKRET(pRandomIndex != nullptr);

  const Uint32 iterations = ctx->getNumLoops() * 10;

  g_err << "Step " << step->getStepTypeNo() << " of "
        << step->getStepTypeCount() << " running " << iterations
        << " scans using index " << iName << " and batchsize " << scanBatchSize
        << endl;

  for (Uint32 i = 0; i < iterations; i++) {
    // g_err << "Step " << step << " iteration " << i << endl;

    NdbTransaction *trans = pNdb->startTransaction();
    CHECKRET(trans != nullptr);

    NdbIndexScanOperation *pOp =
        trans->getNdbIndexScanOperation(iName, ctx->getTab()->getName());
    CHECKRET(pOp != nullptr);

    CHECKRET(pOp->readTuples(NdbScanOperation::LM_CommittedRead,
                             Uint32(0),     /* scan_flags */
                             Uint32(0),     /* parallel */
                             scanBatchSize) /* batch */
             == 0);
    for (int a = 0; a < ctx->getTab()->getNoOfColumns(); a++) {
      CHECKRET(pOp->getValue(a) != nullptr);
    }

    CHECKRET(trans->execute(ExecType::NoCommit) == 0);

    Uint32 rows = 0;
    int rc = 0;
    while ((rc = pOp->nextResult()) == 0) {
      rows++;
    };

    CHECKRET(rc == 1);  // No more tuples;

    trans->close();

    // g_err << "Found " << rows << " rows" << endl;
  }

  ctx->stopTest();

  return NDBT_OK;
}

NDBT_TESTSUITE(testIndex);
TESTCASE("CreateAll",
         "Test that we can create all various indexes on each table\n"
         "Then drop the indexes\n") {
  INITIALIZER(runCreateIndexes);
}
TESTCASE("CreateAll_O",
         "Test that we can create all various indexes on each table\n"
         "Then drop the indexes\n") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runCreateIndexes);
}
TESTCASE("InsertDeleteGentle",
         "Create one index, then perform insert and delete in the table\n"
         "loop number of times. Use batch size 1.") {
  TC_PROPERTY("BatchSize", 1);
  INITIALIZER(runInsertDelete);
  FINALIZER(runClearTable);
}
TESTCASE("InsertDeleteGentle_O",
         "Create one index, then perform insert and delete in the table\n"
         "loop number of times. Use batch size 1.") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", 1);
  INITIALIZER(runInsertDelete);
  FINALIZER(runClearTable);
}
TESTCASE("InsertDelete",
         "Create one index, then perform insert and delete in the table\n"
         "loop number of times. Use batchsize 512 to stress db more") {
  TC_PROPERTY("BatchSize", 512);
  INITIALIZER(runInsertDelete);
  FINALIZER(runClearTable);
}
TESTCASE("InsertDelete_O",
         "Create one index, then perform insert and delete in the table\n"
         "loop number of times. Use batchsize 512 to stress db more") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", 512);
  INITIALIZER(runInsertDelete);
  FINALIZER(runClearTable);
}
TESTCASE("CreateLoadDropGentle",
         "Try to create, drop and load various indexes \n"
         "on table loop number of times.Usa batch size 1.\n") {
  TC_PROPERTY("BatchSize", 1);
  INITIALIZER(runCreateLoadDropIndex);
}
TESTCASE("CreateLoadDropGentle_O",
         "Try to create, drop and load various indexes \n"
         "on table loop number of times.Usa batch size 1.\n") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", 1);
  INITIALIZER(runCreateLoadDropIndex);
}
TESTCASE(
    "CreateLoadDrop",
    "Try to create, drop and load various indexes \n"
    "on table loop number of times. Use batchsize 512 to stress db more\n") {
  TC_PROPERTY("BatchSize", 512);
  INITIALIZER(runCreateLoadDropIndex);
}
TESTCASE(
    "CreateLoadDrop_O",
    "Try to create, drop and load various indexes \n"
    "on table loop number of times. Use batchsize 512 to stress db more\n") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", 512);
  INITIALIZER(runCreateLoadDropIndex);
}
TESTCASE("NFNR1",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("PauseThreads", 2);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions1);
  STEP(runTransactions1);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR1_O",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("PauseThreads", 2);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions1);
  STEP(runTransactions1);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR2",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("PauseThreads", 2);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR2_O",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("PauseThreads", 1);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions2);
  // STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR3",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("PauseThreads", 2);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  STEP(runRestarts);
  STEP(runTransactions3);
  FINALIZER(runVerifyIndex);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR3_O",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("PauseThreads", 2);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  STEP(runRestarts);
  STEP(runTransactions3);
  FINALIZER(runVerifyIndex);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR4",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("PauseThreads", 4);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions1);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR4_O",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("PauseThreads", 4);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions1);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR5",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", (unsigned)1);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runLQHKEYREF);
  STEP(runTransactions1);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR5_O",
         "Test that indexes are correctly maintained during node fail and node "
         "restart") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", (unsigned)1);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runLQHKEYREF);
  STEP(runTransactions1);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("SR1", "Test that indexes are correctly maintained during SR") {
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  STEP(runSystemRestart1);
  FINALIZER(runVerifyIndex);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("MixedTransaction", "Test mixing of index and normal operations") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runMixed1);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("MixedTransaction2",
         "Test mixing of index and normal operations with batching") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runMixed2);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("SR1_O", "Test that indexes are correctly maintained during SR") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  STEP(runSystemRestart1);
  FINALIZER(runVerifyIndex);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("BuildDuring",
         "Test that index build when running transactions work") {
  TC_PROPERTY("OrderedIndex", (unsigned)0);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("Threads", 1);  // # runTransactions4
  TC_PROPERTY("BatchSize", 1);
  INITIALIZER(runClearTable);
  STEP(runBuildDuring);
  STEP(runTransactions4);
  // STEP(runTransactions4);
  FINALIZER(runClearTable);
}
TESTCASE("BuildDuring2",
         "Test that index build when running transactions work") {
  TC_PROPERTY("OrderedIndex", (unsigned)0);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", 1);
  TC_PROPERTY("UntilStopped", Uint32(1));
  INITIALIZER(runClearTable);
  STEP(runBuildDuring);
  STEPS(runMixedDML, 3);
  FINALIZER(runClearTable);
}
TESTCASE("BuildDuring_O",
         "Test that index build when running transactions work") {
  TC_PROPERTY("OrderedIndex", (unsigned)1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("Threads", 1);  // # runTransactions4
  INITIALIZER(runClearTable);
  STEP(runBuildDuring);
  STEP(runTransactions4);
  // STEP(runTransactions4);
  FINALIZER(runClearTable);
}
TESTCASE("UniqueNull", "Test that unique indexes and nulls") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runUniqueNullTransactions);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("Bug21384", "Test that unique indexes and nulls") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runBug21384);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("Bug25059", "Test that unique indexes and nulls") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runBug25059);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("Bug28804",
         "Test behaviour on out of TransactionBufferMemory for index lookup") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runBug28804);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("Bug28804_ATTRINFO",
         "Test behaviour on out of TransactionBufferMemory for index lookup"
         " in saveINDXATTRINFO") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runBug28804_ATTRINFO);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("Bug46069", "") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("THREADS", 12);
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  INITIALIZER(createPkIndex);
  STEP(runBug46069);
  STEPS(runBug46069_pkdel, 10);
  STEPS(runBug46069_scandel, 2);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("ConstraintDetails",
         "Test that the details part of the returned NdbError is as "
         "expected") {
  INITIALIZER(runConstraintDetails);
}
TESTCASE("Bug50118", "") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(runLoadTable);
  INITIALIZER(createPkIndex);
  STEP(runReadIndexUntilStopped);
  STEP(runReadIndexUntilStopped);
  STEP(runReadIndexUntilStopped);
  STEP(runBug50118);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("FireTrigOverload", "") {
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("NotOnlyPkId",
              (unsigned)1);  // Index must be non PK to fire triggers
  TC_PROPERTY(NDBT_TestCase::getStepThreadStackSizePropName(), 128 * 1024);
  INITIALIZER(createRandomIndex);
  INITIALIZER(runClearTable);
  STEP(runTrigOverload);
  FINALIZER(runClearError);
  FINALIZER(createRandomIndex_Drop);
}
TESTCASE("DeferredError",
         "Test with deferred unique index handling and error inserts") {
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  TC_PROPERTY("OrderedIndex", Uint32(0));
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runTestDeferredError);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("DeferredMixedLoad", "Test mixed load of DML with deferred indexes") {
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  TC_PROPERTY("OrderedIndex", Uint32(0));
  TC_PROPERTY("UntilStopped", Uint32(0));
  TC_PROPERTY("Deferred", Uint32(1));
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEPS(runMixedDML, 10);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("DeferredMixedLoadError",
         "Test mixed load of DML with deferred indexes. "
         "Need --skip-ndb-optimized-node-selection") {
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  TC_PROPERTY("OrderedIndex", Uint32(0));
  TC_PROPERTY("UntilStopped", Uint32(1));
  TC_PROPERTY("Deferred", Uint32(1));
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEPS(runMixedDML, 4);
  STEP(runDeferredError);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("NF_DeferredMixed", "Test mixed load of DML with deferred indexes") {
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  TC_PROPERTY("OrderedIndex", Uint32(0));
  TC_PROPERTY("UntilStopped", Uint32(1));
  TC_PROPERTY("Deferred", Uint32(1));
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEPS(runMixedDML, 4);
  STEP(runRestarts);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("NF_Mixed", "Test mixed load of DML") {
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  TC_PROPERTY("OrderedIndex", Uint32(0));
  TC_PROPERTY("UntilStopped", Uint32(1));
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEPS(runMixedDML, 4);
  STEP(runRestarts);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("Bug56829",
         "Return empty ordered index nodes to index fragment "
         "so that empty fragment pages can be freed") {
  STEP(runBug56829);
}
TESTCASE("Bug12315582", "") {
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  TC_PROPERTY("OrderedIndex", Uint32(0));
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug12315582);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("Bug60851", "") {
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  TC_PROPERTY("OrderedIndex", Uint32(0));
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug60851);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("RefreshWithOrderedIndex", "Refresh tuples with ordered index(es)") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  INITIALIZER(createPkIndex);
  INITIALIZER(runRefreshTupleAbort);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("ScanOrderedIndexWithChurn",
         "Concurrent scans while modifications are occurring") {
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", Uint32(0));
  /**
   * Don't include updates column in index as we scan
   * slowly, so ascending result set can be large
   */
  TC_PROPERTY("NotIncludingUpdates", Uint32(1));
  /**
   * Small scan batchsize, to increase chance of scans
   * being in-progress during DML commit
   */
  TC_PROPERTY("ScanBatchSize", Uint32(3));
  INITIALIZER(createRandomIndex);
  INITIALIZER(runLoadTable);
  STEP(runChunkyUpdatesUntilStopped);
  STEP(runChunkyInsertDeletesUntilStopped);
  STEPS(runRandomIndexScan, 10);
  FINALIZER(runClearTable);
  FINALIZER(createRandomIndex_Drop);
}

NDBT_TESTSUITE_END(testIndex)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testIndex);
  return testIndex.execute(argc, argv);
}

template class Vector<Attrib *>;
