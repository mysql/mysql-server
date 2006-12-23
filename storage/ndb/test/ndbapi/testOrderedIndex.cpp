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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <Vector.hpp>
#include <ndbapi_limits.h>

const unsigned MaxTableAttrs = NDB_MAX_ATTRIBUTES_IN_TABLE;
const unsigned MaxIndexAttrs = NDB_MAX_ATTRIBUTES_IN_INDEX;
const unsigned MaxIndexes = 20;

static unsigned
urandom(unsigned n)
{
  unsigned i = random();
  return i % n;
}

static int
runDropIndex(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDic = pNdb->getDictionary();
  NdbDictionary::Dictionary::List list;
  if (pDic->listIndexes(list, pTab->getName()) != 0) {
    g_err << pTab->getName() << ": listIndexes failed" << endl;
    ERR(pDic->getNdbError());
    return NDBT_FAILED;
  }
  for (unsigned i = 0; i < list.count; i++) {
    NDBT_Index* pInd = new NDBT_Index(list.elements[i].name);
    pInd->setTable(pTab->getName());
    g_info << "Drop index:" << endl << *pInd;
    if (pInd->dropIndexInDb(pNdb) != 0) {
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

static Uint32 workaround[1000];

static void
setTableProperty(NDBT_Context* ctx, NDBT_Table* pTab, const char* name, Uint32 num)
{
  char key[200];
  sprintf(key, "%s-%s", name, pTab->getName());
  //ctx->setProperty(key, num);
  workaround[pTab->getTableId()] = num;
}

static Uint32
getTableProperty(NDBT_Context* ctx, NDBT_Table* pTab, const char* name)
{
  char key[200];
  sprintf(key, "%s-%s", name, pTab->getName());
  //Uint32 num = ctx->getProperty(key, (Uint32)-1);
  Uint32 num = workaround[pTab->getTableId()];
  assert(num != (Uint32)-1);
  return num;
}

static int
runCreateIndex(NDBT_Context* ctx, NDBT_Step* step)
{
  srandom(1);
  NDBT_Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  unsigned numTabAttrs = pTab->getNumAttributes();
  unsigned numIndex = 0;
  while (numIndex < MaxIndexes) {
    if (numIndex != 0 && urandom(10) == 0)
      break;
    char buf[200];
    sprintf(buf, "%s_X%03d", pTab->getName(), numIndex);
    NDBT_Index* pInd = new NDBT_Index(buf);
    pInd->setTable(pTab->getName());
    pInd->setType(NdbDictionary::Index::OrderedIndex);
    pInd->setLogging(false);
    unsigned numAttrs = 0;
    while (numAttrs < MaxIndexAttrs) {
      if (numAttrs != 0 && urandom(5) == 0)
	break;
      unsigned i = urandom(numTabAttrs);
      const NDBT_Attribute* pAttr = pTab->getAttribute(i);
      bool found = false;
      for (unsigned j = 0; j < numAttrs; j++) {
	if (strcmp(pAttr->getName(), pInd->getAttribute(j)->getName()) == 0) {
	  found = true;
	  break;
	}
      }
      if (found)
	continue;
      pInd->addAttribute(*pAttr);
      numAttrs++;
    }
    g_info << "Create index:" << endl << *pInd;
    if (pInd->createIndexInDb(pNdb, false) != 0)
      continue;
    numIndex++;
  }
  setTableProperty(ctx, pTab, "numIndex", numIndex);
  g_info << "Created " << numIndex << " indexes on " << pTab->getName() << endl;
  return NDBT_OK;
}

static int
runInsertUpdate(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  int ret;
  g_info << "Insert: " << pTab->getName() << endl;
  HugoTransactions hugoTrans(*pTab);
  ret = hugoTrans.loadTable(pNdb, ctx->getNumRecords(), 100);
  if (ret != 0) {
    g_err << "ERR: " << step->getName() << "failed" << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

static int
runFullScan(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  unsigned cntIndex = getTableProperty(ctx, pTab, "numIndex");
  for (unsigned numIndex = 0; numIndex < cntIndex; numIndex++) {
    char buf[200];
    sprintf(buf, "%s_X%03d", pTab->getName(), numIndex);
    NDBT_Index* pInd = NDBT_Index::discoverIndexFromDb(pNdb, buf, pTab->getName());
    assert(pInd != 0);
    g_info << "Scan index:" << pInd->getName() << endl << *pInd;
    NdbConnection* pCon = pNdb->startTransaction();
    if (pCon == 0) {
      ERR(pNdb->getNdbError());
      return NDBT_FAILED;
    }
    NdbOperation* pOp = pCon->getNdbOperation(pInd->getName(),
					      pTab->getName());
    if (pOp == 0) {
      ERR(pCon->getNdbError());
      pNdb->closeTransaction(pCon);
      return NDBT_FAILED;
    }
    if (pOp->openScanRead() != 0) {
      ERR(pCon->getNdbError());
      pNdb->closeTransaction(pCon);
      return NDBT_FAILED;
    }
    if (pCon->executeScan() != 0) {
      ERR(pCon->getNdbError());
      pNdb->closeTransaction(pCon);
      return NDBT_FAILED;
    }
    unsigned rows = 0;
    while (1) {
      int ret = pCon->nextScanResult();
      if (ret == 0) {
        rows++;
      } else if (ret == 1) {
        break;
      } else {
        ERR(pCon->getNdbError());
        pNdb->closeTransaction(pCon);
        return NDBT_FAILED;
      }
    }
    pNdb->closeTransaction(pCon);
    g_info << "Scanned " << rows << " rows" << endl;
  }
  return NDBT_OK;
}

NDBT_TESTSUITE(testOrderedIndex);
TESTCASE(
    "DropIndex",
    "Drop any old indexes") {
  INITIALIZER(runDropIndex);
}
TESTCASE(
    "CreateIndex",
    "Create ordered indexes") {
  INITIALIZER(runCreateIndex);
}
TESTCASE(
    "InsertUpdate",
    "Run inserts and updates") {
  INITIALIZER(runInsertUpdate);
}
TESTCASE(
    "FullScan",
    "Full scan on each ordered index") {
  INITIALIZER(runFullScan);
}
NDBT_TESTSUITE_END(testOrderedIndex);

int
main(int argc, const char** argv)
{
  ndb_init();
  return testOrderedIndex.execute(argc, argv);
}

// vim: set sw=2:
