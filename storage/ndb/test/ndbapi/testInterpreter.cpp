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

#include <NdbTick.h>
#include <random.h>
#include <HugoTransactions.hpp>
#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <UtilTransactions.hpp>
#include <Vector.hpp>
#include <cstring>
#include "util/require.h"

#define CHK_RET_FAILED(x, y)                                   \
  if (!(x)) {                                                  \
    ndbout_c("Failed on line: %u.  Error %u %s.", __LINE__,    \
             y->getNdbError().code, y->getNdbError().message); \
    return NDBT_FAILED;                                        \
  }

#define CHK2(x, y)                                             \
  if (x == -1) {                                               \
    ndbout_c("Failed on line: %u.  Error %u %s.", __LINE__,    \
             y->getNdbError().code, y->getNdbError().message); \
    pTrans->close();                                           \
    return NDBT_FAILED;                                        \
  }

int runClearTable(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.pkDelRecords(GETNDB(step), records, batchSize) != 0) {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runLoadTable(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0) {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

/**
 * Read the data back, HugoTransactions will check it for sanity
 */
int runCheckData(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());

  int checkDoubleOption = ctx->getProperty("CheckDouble", Uint32(0));

  if (checkDoubleOption != 0) {
    records *= 2;
  }

  ndbout_c("Checking %u records", records);

  // Verify the data
  if (hugoTrans.pkReadRecords(GETNDB(step), records) != 0) {
    return NDBT_FAILED;
  }

  ndbout_c("Ok");

  return NDBT_OK;
}

int runCheckUpdatesValue(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  int records = ctx->getNumRecords();
  HugoOperations hugoOps(*ctx->getTab());

  if (hugoOps.startTransaction(pNdb) != 0) {
    return NDBT_FAILED;
  }

  if (hugoOps.pkReadRecord(pNdb, 0, records) != 0) {
    ndbout_c("Failed to read record");
    return NDBT_FAILED;
  }

  if (hugoOps.execute_Commit(pNdb) != 0) {
    ndbout_c("Failed to execute read");
    hugoOps.closeTransaction(pNdb);
    return NDBT_FAILED;
  }

  // Default updates value indicating the update operation did not succeed
  // in the previous STEP
  int updatesValue = ctx->getProperty("UpdatesValue");
  int result = hugoOps.verifyUpdatesValue(updatesValue, records);

  hugoOps.closeTransaction(pNdb);
  return result;
}

int runTestIncValue64(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();
  //  NDBT_Table* pTab = ctx->getTab();
  // Ndb* pNdb = GETNDB(step);

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.pkInterpretedUpdateRecords(GETNDB(step), records) != 0) {
    return NDBT_FAILED;
  }

  // Verify the update
  if (hugoTrans.pkReadRecords(GETNDB(step), records) != 0) {
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runTestIncValue32(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);

  if (strcmp(pTab->getName(), "T1") != 0) {
    ndbout_c("runTestIncValue32: skip, table != T1");
    return NDBT_OK;
  }

  NdbConnection *pTrans = pNdb->startTransaction();
  if (pTrans == NULL) {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  NdbOperation *pOp = pTrans->getNdbOperation(pTab->getName());
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  int check = pOp->interpretedUpdateTuple();
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Primary keys
  Uint32 pkVal = 1;
  check = pOp->equal("KOL1", pkVal);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Attributes

  // Perform initial read of column start value
  NdbRecAttr *initialVal = pOp->getValue("KOL2");
  if (initialVal == NULL) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Update the column
  Uint32 valToIncWith = 1;
  check = pOp->incValue("KOL2", valToIncWith);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Perform final read of column after value
  NdbRecAttr *afterVal = pOp->getValue("KOL2");
  if (afterVal == NULL) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  check = pTrans->execute(Commit);
  if (check == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  Uint32 oldValue = initialVal->u_32_value();
  Uint32 newValue = afterVal->u_32_value();
  Uint32 expectedValue = oldValue + valToIncWith;

  if (newValue != expectedValue) {
    g_err << "Failed : Expected " << oldValue << "+" << valToIncWith << "="
          << expectedValue << " but received " << newValue << endl;
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  pNdb->closeTransaction(pTrans);

  return NDBT_OK;
}

int runTestBug19537(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);

  if (strcmp(pTab->getName(), "T1") != 0) {
    g_err << "runTestBug19537: skip, table != T1" << endl;
    return NDBT_OK;
  }

  NdbConnection *pTrans = pNdb->startTransaction();
  if (pTrans == NULL) {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  NdbOperation *pOp = pTrans->getNdbOperation(pTab->getName());
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pOp->interpretedUpdateTuple() == -1) {
    NDB_ERR(pOp->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Primary keys
  const Uint32 pkVal = 1;
  if (pOp->equal("KOL1", pkVal) == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Load 64-bit constant into register 1 and
  // write from register 1 to 32-bit column KOL2
  const Uint64 reg_val = 0x0102030405060708ULL;
#if 0
  Uint32 reg_ptr32[2];
  memcpy(&(reg_ptr32[0]), (Uint8*)&reg_val, sizeof(Uint32));
  memcpy(&(reg_ptr32[1]), ((Uint8*)&reg_val)+4, sizeof(Uint32));
  if (reg_ptr32[0] == 0x05060708 && reg_ptr32[1] == 0x01020304) {
    g_err << "runTestBug19537: platform is LITTLE endian" << endl;
  } else if (reg_ptr32[0] == 0x01020304 && reg_ptr32[1] == 0x05060708) {
    g_err << "runTestBug19537: platform is BIG endian" << endl;
  } else {
    g_err << "runTestBug19537: impossible platform"
          << hex << " [0]=" << reg_ptr32[0] << " [1]=" <<reg_ptr32[1] << endl;
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }
#endif

  if (pOp->load_const_u64(1, reg_val) == -1 ||
      pOp->write_attr("KOL2", 1) == -1) {
    NDB_ERR(pOp->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pTrans->execute(Commit) == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Read value via a new transaction

  pTrans = pNdb->startTransaction();
  if (pTrans == NULL) {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  pOp = pTrans->getNdbOperation(pTab->getName());
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  Uint32 kol2 = 0x09090909;
  if (pOp->readTuple() == -1 || pOp->equal("KOL1", pkVal) == -1 ||
      pOp->getValue("KOL2", (char *)&kol2) == 0) {
    NDB_ERR(pOp->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pTrans->execute(Commit) == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Expected conversion as in C - truncate to lower (logical) word

  if (kol2 == 0x01020304) {
    g_err << "runTestBug19537: the bug manifests itself !" << endl;
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (kol2 != 0x05060708) {
    g_err << "runTestBug19537: impossible KOL2 " << hex << kol2 << endl;
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  pNdb->closeTransaction(pTrans);
  return NDBT_OK;
}

int runTestBug34107(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  const Uint32 okSize = 10000;
  const Uint32 tooBig = 30000;

  Uint32 codeBuff[tooBig];

  int i;
  for (i = 0; i <= 1; i++) {
    g_info << "bug34107:" << (i == 0 ? " small" : " too big") << endl;

    NdbConnection *pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      NDB_ERR(pNdb->getNdbError());
      return NDBT_FAILED;
    }

    NdbScanOperation *pOp = pTrans->getNdbScanOperation(pTab->getName());
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (pOp->readTuples() == -1) {
      NDB_ERR(pOp->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    /* Test kernel mechanism for dealing with too large program
     * We need to provide our own program buffer as default
     * NdbInterpretedCode buffer will not grow larger than
     * NDB_MAX_SCANFILTER_SIZE
     */

    NdbInterpretedCode code(NULL,  // Table is irrelevant
                            codeBuff,
                            tooBig);  // Size of codeBuff

    int n = i == 0 ? okSize : tooBig;
    int k;

    for (k = 0; k < n; k++) {
      // inserts 1 word ATTRINFO

      if (code.interpret_exit_ok() == -1) {
        NDB_ERR(code.getNdbError());
        pNdb->closeTransaction(pTrans);
        return NDBT_FAILED;
      }
    }

    if (code.finalise() != 0) {
      NDB_ERR(code.getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (pOp->setInterpretedCode(&code) != 0) {
      NDB_ERR(pOp->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (pTrans->execute(NoCommit) == -1) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    int ret;
    while ((ret = pOp->nextResult()) == 0)
      ;
    g_info << "ret=" << ret << " err=" << pOp->getNdbError().code << endl;

    if (i == 0 && ret != 1) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (i == 1 && ret != -1) {
      g_err << "unexpected big filter success" << endl;
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    if (i == 1 && pOp->getNdbError().code != 874) {
      g_err << "unexpected big filter error code, wanted 874" << endl;
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    pNdb->closeTransaction(pTrans);
  }

  return NDBT_OK;
}

static char pkIdxName[256];

int createPkIndex(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);

  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);
  bool logged = ctx->getProperty("LoggedIndexes", (Uint32)0);
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

int runInterpretedUKLookup(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();

  const NdbDictionary::Index *pIdx = dict->getIndex(pkIdxName, pTab->getName());
  CHK_RET_FAILED(pIdx != 0, dict);

  const NdbRecord *pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0, dict);
  const NdbRecord *pIdxRecord = pIdx->getDefaultRecord();
  CHK_RET_FAILED(pIdxRecord != 0, dict);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 *pRow = new Uint8[len];
  std::memset(pRow, 0, len);

  HugoCalculator calc(*pTab);
  calc.equalForRow(pRow, pRowRecord, 0);

  NdbTransaction *pTrans = pNdb->startTransaction();
  CHK_RET_FAILED(pTrans != 0, pNdb);

  NdbInterpretedCode code;
  code.interpret_exit_ok();
  code.finalise();

  NdbOperation::OperationOptions opts;
  std::memset(&opts, 0, sizeof(opts));
  opts.optionsPresent = NdbOperation::OperationOptions::OO_INTERPRETED;
  opts.interpretedCode = &code;

  const NdbOperation *pOp =
      pTrans->readTuple(pIdxRecord, (char *)pRow, pRowRecord, (char *)pRow,
                        NdbOperation::LM_Read, 0, &opts, sizeof(opts));
  CHK_RET_FAILED(pOp, pTrans);
  int res = pTrans->execute(Commit, AbortOnError);

  CHK_RET_FAILED(res == 0, pTrans);

  delete[] pRow;

  return NDBT_OK;
}

int runTestBranchNonZeroLabel(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);

  // Find first find Bit column
  bool found = false;
  int colId = 0;
  while (colId < pTab->getNoOfColumns()) {
    const NdbDictionary::Column *col = pTab->getColumn(colId);
    if (col->getType() == NdbDictionary::Column::Bit) {
      ndbout << "Found first Bit column " << colId << " " << col->getName()
             << endl;
      found = true;
      break;
    }
    colId++;
  }
  if (!found) {
    ndbout << "Test skipped since no Bit column found in table "
           << pTab->getName() << endl;
    return NDBT_OK;
  }

  NdbConnection *pTrans = pNdb->startTransaction();
  if (pTrans == NULL) {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  const Uint32 numWords = 64;
  Uint32 space[numWords];
  NdbInterpretedCode stackCode(pTab, &space[0], numWords);

  NdbInterpretedCode *code = &stackCode;

  constexpr int label_0 = 0;
  constexpr int label_1 = 1;
  constexpr int label_2 = 2;
  const Uint32 mask = myRandom48(UINT32_MAX);
  const Uint32 op = myRandom48(4);

  /*
   * This test only verifies that label argument to branch_col_and_mask_eq_mask
   * is looked at, if not an internal loop will result
   */
  if (code->def_label(label_0) == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  ndbout << "Operation " << op << " mask " << hex << mask << endl;
  int ret;
  switch (op) {
    case 0:
      ret = code->branch_col_and_mask_eq_mask(&mask, sizeof(mask), colId,
                                              label_2);
      break;
    case 1:
      ret = code->branch_col_and_mask_ne_mask(&mask, sizeof(mask), colId,
                                              label_2);
      break;
    case 2:
      ret = code->branch_col_and_mask_eq_zero(&mask, sizeof(mask), colId,
                                              label_2);
      break;
    case 3:
      ret = code->branch_col_and_mask_ne_zero(&mask, sizeof(mask), colId,
                                              label_2);
      break;
    default:
      abort();
  }
  if (ret == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (code->def_label(label_1) == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (code->interpret_exit_nok() == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (code->def_label(label_2) == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (code->interpret_exit_ok() == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (code->finalise() == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  NdbScanOperation *pOp = pTrans->getNdbScanOperation(pTab->getName());
  if (pOp == NULL) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pOp->readTuples() == -1) {
    NDB_ERR(pOp->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pOp->setInterpretedCode(code) == -1) {
    NDB_ERR(pOp->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pTrans->execute(NoCommit) == -1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  size_t rows = 0;
  while ((ret = pOp->nextResult()) == 0) rows++;
  g_info << "rows=" << rows << " ret=" << ret
         << " err=" << pOp->getNdbError().code << endl;

  if (ret != 1) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  pNdb->closeTransaction(pTrans);

  return NDBT_OK;
}

/**
 * Run interpreted update on all the records, with
 * optional extra getValues
 * Check returned data.
 */
int runInterpretedUpdate(NDBT_Context *ctx, NDBT_Step *step) {
  const Uint32 records = (Uint32)ctx->getNumRecords();
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();
  HugoCalculator calc(*pTab);

  const NdbRecord *pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0, dict);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 *pRow = new Uint8[len];
  std::memset(pRow, 0, len);

  for (Uint32 r = 0; r < records; r++) {
    /* Modified updates value to 1 */
    calc.setValues(pRow, pRowRecord, r, 1);

    NdbTransaction *pTrans = pNdb->startTransaction();
    CHK_RET_FAILED(pTrans != 0, pNdb);

    NdbOperation::OperationOptions opts;
    std::memset(&opts, 0, sizeof(opts));

    NdbOperation::GetValueSpec getvals[NDB_MAX_ATTRIBUTES_IN_TABLE];
    NDBT_ResultRow initialRead(*pTab);
    int skipInitialReadOption = ctx->getProperty("SkipInitialRead", Uint32(0));
    if (skipInitialReadOption == 0) {
      /* Define 'extra getvalues' */
      for (int k = 0; k < pTab->getNoOfColumns(); k++) {
        getvals[k].column = pTab->getColumn(k);
        getvals[k].appStorage = NULL;
        getvals[k].recAttr = NULL;
      }

      opts.optionsPresent |= NdbOperation::OperationOptions::OO_GETVALUE;
      opts.extraGetValues = getvals;
      opts.numExtraGetValues = pTab->getNoOfColumns();
    }

    NdbInterpretedCode code;
    code.interpret_exit_ok();
    code.finalise();

    opts.optionsPresent |= NdbOperation::OperationOptions::OO_INTERPRETED;
    opts.interpretedCode = &code;

    ndbout_c("Executing interpreted update on row %u", r);

    const NdbOperation *pOp =
        pTrans->updateTuple(pRowRecord, (char *)pRow, pRowRecord, (char *)pRow,
                            NULL, &opts, sizeof(opts));
    CHK_RET_FAILED(pOp, pTrans);

    if (skipInitialReadOption == 0) {
      /* Check extra GetValue recAttrs */
      for (int k = 0; k < pTab->getNoOfColumns(); k++) {
        if (getvals[k].recAttr == NULL) {
          abort();
        };
        if (getvals[k].recAttr->isNULL() != -1) {
          abort();
        };
        initialRead.attributeStore(k) = getvals[k].recAttr;
      }
    }

    int res = pTrans->execute(Commit, AbortOnError);

    CHK_RET_FAILED(res == 0, pTrans);

    /* For update, before values should be sane */
    /* Check data read, if any */
    if (skipInitialReadOption == 0) {
      if (calc.verifyRowValues(&initialRead) != 0) {
        ndbout_c("Failed checking initial read for row %u", r);
        pTrans->close();
        return NDBT_FAILED;
      }
      /* Check we got the before value */
      if (calc.getUpdatesValue(&initialRead) != 0) {
        ndbout_c("Incorrect initial updates value for row %u is %u", r,
                 calc.getUpdatesValue(&initialRead));
        pTrans->close();
        return NDBT_FAILED;
      }
      ndbout_c("  Write->Update initial reads ok");
    }

    pTrans->close();
  }

  delete[] pRow;

  return NDBT_OK;
}

/**
 * Run interpreted write on double the records, with
 * optional extra getValues (initial read)
 * 0..records-1 will map to UPDATE
 * record..2*records - 1 will map to INSERT
 * Check returned data is correct, or undefined
 */
int runInterpretedWrite(NDBT_Context *ctx, NDBT_Step *step) {
  const Uint32 records = (Uint32)ctx->getNumRecords();
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();
  HugoCalculator calc(*pTab);

  const NdbRecord *pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0, dict);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 *pRow = new Uint8[len];
  std::memset(pRow, 0, len);

  for (Uint32 r = 0; r < records * 2; r++) {
    /* Modified updates value to 1 */
    calc.setValues(pRow, pRowRecord, r, 1);

    NdbTransaction *pTrans = pNdb->startTransaction();
    CHK_RET_FAILED(pTrans != 0, pNdb);

    NdbOperation::OperationOptions opts;
    std::memset(&opts, 0, sizeof(opts));

    NdbOperation::GetValueSpec getvals[NDB_MAX_ATTRIBUTES_IN_TABLE];
    NDBT_ResultRow initialRead(*pTab);
    int skipInitialReadOption = ctx->getProperty("SkipInitialRead", Uint32(0));
    if (skipInitialReadOption == 0) {
      /* Define 'extra getvalues' */
      for (int k = 0; k < pTab->getNoOfColumns(); k++) {
        getvals[k].column = pTab->getColumn(k);
        getvals[k].appStorage = NULL;
        getvals[k].recAttr = NULL;
      }

      opts.optionsPresent |= NdbOperation::OperationOptions::OO_GETVALUE;
      opts.extraGetValues = getvals;
      opts.numExtraGetValues = pTab->getNoOfColumns();
    }

    NdbInterpretedCode code;
    code.interpret_exit_ok();
    code.finalise();

    opts.optionsPresent |= NdbOperation::OperationOptions::OO_INTERPRETED;
    opts.interpretedCode = &code;

    const bool expectUpdate = (r < records);
    ndbout_c("Executing interpreted write on row %u %s", r,
             (expectUpdate ? "UPDATE" : "INSERT"));

    const NdbOperation *pOp =
        pTrans->writeTuple(pRowRecord, (char *)pRow, pRowRecord, (char *)pRow,
                           NULL, &opts, sizeof(opts));
    CHK_RET_FAILED(pOp, pTrans);

    if (skipInitialReadOption == 0) {
      /* Check extra GetValue recAttrs */
      for (int k = 0; k < pTab->getNoOfColumns(); k++) {
        if (getvals[k].recAttr == NULL) {
          abort();
        };
        if (getvals[k].recAttr->isNULL() != -1) {
          abort();
        };
        initialRead.attributeStore(k) = getvals[k].recAttr;
      }
    }

    int res = pTrans->execute(Commit, AbortOnError);

    CHK_RET_FAILED(res == 0, pTrans);

    if (expectUpdate) {
      /* For update, before values should be sane */
      /* Check data read, if any */
      if (skipInitialReadOption == 0) {
        if (calc.verifyRowValues(&initialRead) != 0) {
          ndbout_c("Failed checking initial read for row %u", r);
          pTrans->close();
          return NDBT_FAILED;
        }
        /* Check we got the before value */
        if (calc.getUpdatesValue(&initialRead) != 0) {
          ndbout_c("Incorrect initial updates value for row %u is %u", r,
                   calc.getUpdatesValue(&initialRead));
          pTrans->close();
          return NDBT_FAILED;
        }
        ndbout_c("  Write->Update initial reads ok");
      }
    } else {
      /**
       * For insert there should be no data read back
       * RecAttrs should be undefined
       */
      if (skipInitialReadOption == 0) {
        for (int k = 0; k < pTab->getNoOfColumns(); k++) {
          if (initialRead.attributeStore(k)->isNULL() != -1) {
            ndbout_c("Initial read of row %u column %u not undefined", r, k);
            pTrans->close();
            return NDBT_FAILED;
          }
        }
        ndbout_c("  Write->Insert initial reads ok");
      }
    }

    pTrans->close();
  }

  delete[] pRow;

  return NDBT_OK;
}

/**
 * Run interpreted write on double the records, with
 * optional extra getValues (initial read and final read)
 * 0..records-1 will map to UPDATE
 * record..2*records - 1 will map to INSERT
 * Check returned data is correct, or undefined
 */
int runInterpretedWriteOldApi(NDBT_Context *ctx, NDBT_Step *step) {
  const Uint32 records = (Uint32)ctx->getNumRecords();
  const NdbDictionary::Table *pTab = ctx->getTab();
  HugoCalculator calc(*pTab);
  Ndb *pNdb = GETNDB(step);

  HugoOperations hugoOps(*pTab);

  for (Uint32 r = 0; r < records * 2; r++) {
    /* Modified updates value to 1 */

    NdbTransaction *pTrans = pNdb->startTransaction();
    CHK_RET_FAILED(pTrans != 0, pNdb);

    NdbOperation *pOp = pTrans->getNdbOperation(pTab->getName());
    CHK_RET_FAILED(pOp, pTrans);

    int check = pOp->interpretedWriteTuple();
    CHK2(check, pTrans);

    /* Set key values, out of order */
    for (int k = pTab->getNoOfColumns() - 1; k >= 0; k--) {
      if (pTab->getColumn(k)->getPrimaryKey()) {
        if (hugoOps.equalForAttr(pOp, k, r) != 0) {
          ndbout_c("Error defining row %u key col %d %u %s", r, k,
                   hugoOps.getNdbError().code, hugoOps.getNdbError().message);
          pTrans->close();
          return NDBT_FAILED;
        }
      }
    }

    NDBT_ResultRow initialRead(*pTab);
    int skipInitialReadOption = ctx->getProperty("SkipInitialRead", Uint32(0));
    if (skipInitialReadOption == 0) {
      for (int k = 0; k < pTab->getNoOfColumns(); k++) {
        NdbRecAttr *recAttr = pOp->getValue(k);
        CHK_RET_FAILED(recAttr, pOp);
        // RecAttr must be undefined (isNULL() == -1)
        if (recAttr->isNULL() != -1) {
          abort();
        };
        initialRead.attributeStore(k) = recAttr;
      }
    }

    int skipProgramOption = ctx->getProperty("SkipProgram", Uint32(0));

    if (skipProgramOption == 0) {
      /* Interpreted program */
      CHK_RET_FAILED((pOp->branch_col_eq_null(0, 0) == 0), pOp);
      CHK_RET_FAILED((pOp->def_label(0) == 0), pOp);
      CHK_RET_FAILED((pOp->interpret_exit_ok() == 0), pOp);
    }

    /* Set values */
    if (hugoOps.setNonPkValues(pOp, r, 1) != 0) {
      ndbout_c("Error setting non pk values for row %u", r);
      pTrans->close();
      return NDBT_FAILED;
    }

    NDBT_ResultRow finalRead(*pTab);
    int skipFinalReadOption = ctx->getProperty("SkipFinalRead", Uint32(0));
    if (skipFinalReadOption == 0) {
      for (int k = 0; k < pTab->getNoOfColumns(); k++) {
        NdbRecAttr *recAttr = pOp->getValue(k);
        CHK_RET_FAILED(recAttr, pOp);
        // RecAttr must be undefined (isNULL() == -1)
        if (recAttr->isNULL() != -1) {
          abort();
        };
        finalRead.attributeStore(k) = recAttr;
      }
    }

    const bool expectUpdate = (r < records);
    ndbout_c("Executing interpreted write on row %u %s", r,
             (expectUpdate ? "UPDATE" : "INSERT"));

    int res = pTrans->execute(Commit, AbortOnError);

    CHK_RET_FAILED(res == 0, pTrans);

    if (expectUpdate) {
      /* For update, before and after values should be sane */
      /* Check data read, if any */
      if (skipInitialReadOption == 0) {
        if (calc.verifyRowValues(&initialRead) != 0) {
          ndbout_c("Failed checking initial read for row %u", r);
          pTrans->close();
          return NDBT_FAILED;
        }
        /* Check we got the before value */
        if (calc.getUpdatesValue(&initialRead) != 0) {
          ndbout_c("Incorrect initial updates value for row %u is %u", r,
                   calc.getUpdatesValue(&initialRead));
          pTrans->close();
          return NDBT_FAILED;
        }
        ndbout_c("  Write->Update initial reads ok");
      }
      if (skipFinalReadOption == 0) {
        if (calc.verifyRowValues(&finalRead) != 0) {
          ndbout_c("Failed checking final read for row %u", r);
          pTrans->close();
          return NDBT_FAILED;
        }
        /* Check we got the after value */
        if (calc.getUpdatesValue(&finalRead) != 1) {
          ndbout_c("Incorrect final updates value for row %u is %u", r,
                   calc.getUpdatesValue(&finalRead));
          pTrans->close();
          return NDBT_FAILED;
        }
        ndbout_c("  Write->Update final reads ok");
      }
    } else {
      /**
       * For insert there should be no data read back
       * RecAttrs should be undefined
       */
      if (skipInitialReadOption == 0) {
        for (int k = 0; k < pTab->getNoOfColumns(); k++) {
          if (initialRead.attributeStore(k)->isNULL() != -1) {
            ndbout_c("Initial read of row %u column %u not undefined", r, k);
            pTrans->close();
            return NDBT_FAILED;
          }
        }
        ndbout_c("  Write->Insert initial reads ok");
      }

      if (skipFinalReadOption == 0) {
        for (int k = 0; k < pTab->getNoOfColumns(); k++) {
          if (finalRead.attributeStore(k)->isNULL() != -1) {
            ndbout_c("Final read of row %u column %u is not undefined", r, k);
            pTrans->close();
            return NDBT_FAILED;
          }
        }
        ndbout_c("  Write->Insert final reads ok");
      }
    }

    pTrans->close();
  }

  return NDBT_OK;
}

/**
 * Run interpreted write with a failing interpreted program and an
 * update operation to show that the transaction fails.
 * To verify it, check the updatesValue of the record which remains
 * unchanged.
 */
int runInterpretedWriteOldApiFail(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  HugoOperations hugoOps(*pTab);

  NdbTransaction *pTrans = pNdb->startTransaction();
  CHK_RET_FAILED(pTrans != 0, pNdb);

  NdbOperation *pOp = pTrans->getNdbOperation(pTab->getName());
  CHK_RET_FAILED(pOp, pTrans);

  int check = pOp->interpretedWriteTuple();
  CHK2(check, pTrans);

  int r = 0;
  for (int k = 0; k < pTab->getNoOfColumns(); k++) {
    if (pTab->getColumn(k)->getPrimaryKey()) {
      if (hugoOps.equalForAttr(pOp, k, r) != 0) {
        ndbout_c("Error defining row %u key col %d %u %s", r, k,
                 hugoOps.getNdbError().code, hugoOps.getNdbError().message);
        pTrans->close();
        return NDBT_FAILED;
      }
    }
  }

  /* A failing interpreted program */
  CHK_RET_FAILED((pOp->branch_col_eq_null(0, 0) == 0), pOp);
  CHK_RET_FAILED((pOp->def_label(0) == 0), pOp);
  CHK_RET_FAILED((pOp->interpret_exit_nok() == 0), pOp);

  /* Set values */
  if (hugoOps.setNonPkValues(pOp, r, 1) != 0) {
    ndbout_c("Error setting non pk values for row %u", r);
    pTrans->close();
    return NDBT_FAILED;
  }

  int res = pTrans->execute(Commit, AbortOnError);
  if (!(res == -1 && pTrans->getNdbError().code == 899)) {
    ndbout_c("Failed with an unexpected error %d!", pTrans->getNdbError().code);
    return NDBT_FAILED;
  }

  ndbout_c("Failed as expected since the interpreted program failed!");
  pTrans->close();
  return NDBT_OK;
}

int runInterpretedWriteInsert(NDBT_Context *ctx, NDBT_Step *step) {
  const int acceptError = ctx->getProperty("AcceptError");
  const NdbDictionary::Table *pTab = ctx->getTab();
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *dict = pNdb->getDictionary();

  const NdbRecord *pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0, dict);

  /* Initialise content for existing row 0 */
  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 *pRow = new Uint8[len];
  std::memset(pRow, 0, len);

  ndbout_c("Attempting to define interpreted insert");
  HugoCalculator calc(*pTab);
  calc.equalForRow(pRow, pRowRecord, 0);

  NdbTransaction *pTrans = pNdb->startTransaction();
  CHK_RET_FAILED(pTrans != 0, pNdb);

  NdbInterpretedCode code;
  code.interpret_exit_ok();
  code.finalise();

  NdbOperation::OperationOptions opts;
  std::memset(&opts, 0, sizeof(opts));
  opts.optionsPresent = NdbOperation::OperationOptions::OO_INTERPRETED;
  opts.interpretedCode = &code;

  const NdbOperation *pOp =
      pTrans->insertTuple(pRowRecord, (char *)pRow, pRowRecord, (char *)pRow,
                          NULL, &opts, sizeof(opts));
  if (!pOp) {
    if (pTrans->getNdbError().code != acceptError) {
      // Expect the operation to fail because the
      // interpretedWrite program does not run in the INSERT case.
      ndbout_c("Expected error: %d", acceptError);
      return NDBT_FAILED;
    }
  }
  pTrans->close();

  ndbout_c("Failed with error %d as expected!", acceptError);
  delete[] pRow;

  return NDBT_OK;
}

int runInterpretedWriteProgram(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *pTab = ctx->getTab();
  if (strcmp(pTab->getName(), "T1") != 0) {
    ndbout_c("runInterpretedWriteProgram: skip, table != T1");
    return NDBT_OK;
  }

  /* Register aliases */
  const Uint32 R1 = 1, R2 = 2;
  const char *colname = "KOL4";
  Ndb *pNdb = GETNDB(step);

  NdbConnection *pTrans = pNdb->startTransaction();
  CHK_RET_FAILED(pTrans != 0, pNdb);

  NdbOperation *pOp = pTrans->getNdbOperation(pTab->getName());
  CHK_RET_FAILED(pOp, pTrans);

  int check = pOp->interpretedWriteTuple();
  CHK2(check, pTrans);

  // Primary key
  Uint32 pkVal = 999;
  check = pOp->equal("KOL1", pkVal);
  CHK2(check, pTrans);

  // Perform initial read of column start value
  NdbRecAttr *initialVal = pOp->getValue(colname);
  if (initialVal == NULL) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }
  // RecAttr must be undefined (isNULL() == -1)
  if (initialVal->isNULL() != -1) {
    abort();
  };

  Uint32 valToIncWith = 300;
  check = pOp->load_const_u32(R2, valToIncWith);
  CHK2(check, pOp);

  check = pOp->read_attr(colname, R1);
  CHK2(check, pOp);

  Uint32 comparisonValue = 0;
  // if(comparisonValue < KOL4's value) go to Label 0;
  // KOL4 has a non-zero value which makes the condition to evaluate to Label 0
  check = pOp->branch_col_lt(pTab->getColumn(colname)->getColumnNo(),
                             &comparisonValue, sizeof(Uint32), false, 0);
  CHK2(check, pOp);

  check = pOp->interpret_exit_nok(626);
  CHK2(check, pOp);

  // Label 0
  check = pOp->def_label(0);
  CHK_RET_FAILED(check == 0, pOp);

  check = pOp->add_reg(R1, R2, R1);
  CHK2(check, pOp);

  check = pOp->write_attr(colname, R1);
  CHK2(check, pOp);

  // Perform final read of column after value
  NdbRecAttr *afterVal = pOp->getValue(colname);
  if (afterVal == NULL) {
    NDB_ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }
  check = pTrans->execute(Commit);
  CHK2(check, pTrans);

  Uint32 oldValue = initialVal->u_32_value();
  Uint32 newValue = afterVal->u_32_value();
  Uint32 expectedValue = oldValue + valToIncWith;

  pTrans->close();
  const bool result = newValue == expectedValue;
  ndbout_c("Expected %d + %d = %d. Received %d : %s!", oldValue, valToIncWith,
           expectedValue, newValue, (result) ? "Passed" : "Failed");
  if (!result) return NDBT_FAILED;

  return NDBT_OK;
}

NDBT_TESTSUITE(testInterpreter);
TESTCASE("IncValue32", "Test incValue for 32 bit integer\n") {
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestIncValue32);
  FINALIZER(runClearTable);
}
TESTCASE("IncValue64", "Test incValue for 64 bit integer\n") {
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestIncValue64);
  FINALIZER(runClearTable);
}
TESTCASE("Bug19537", "Test big-endian write_attr of 32 bit integer\n") {
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestBug19537);
  FINALIZER(runClearTable);
}
TESTCASE("Bug34107", "Test too big scan filter (error 874)\n") {
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestBug34107);
  FINALIZER(runClearTable);
}
TESTCASE("BranchNonZeroLabel", "Test branch labels with and_mask op\n") {
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestBranchNonZeroLabel);
  FINALIZER(runClearTable);
}
#if 0
TESTCASE("MaxTransactions", 
	 "Start transactions until no more can be created\n"){ 
  INITIALIZER(runTestMaxTransaction);
}
TESTCASE("MaxOperations", 
	"Get operations until no more can be created\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestMaxOperations);
  FINALIZER(runClearTable);
}
TESTCASE("MaxGetValue", 
	"Call getValue loads of time\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestGetValue);
  FINALIZER(runClearTable);
}
TESTCASE("MaxEqual", 
	"Call equal loads of time\n"){ 
  INITIALIZER(runTestEqual);
}
TESTCASE("DeleteNdb", 
	"Make sure that a deleted Ndb object is properly deleted\n"
	"and removed from transporter\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestDeleteNdb);
  FINALIZER(runClearTable);
}
TESTCASE("WaitUntilReady", 
	"Make sure you get an error message when calling waitUntilReady\n"
	"without an init'ed Ndb\n"){ 
  INITIALIZER(runTestWaitUntilReady);
}
TESTCASE("GetOperationNoTab", 
	"Call getNdbOperation on a table that does not exist\n"){ 
  INITIALIZER(runGetNdbOperationNoTab);
}
TESTCASE("MissingOperation", 
	"Missing operation request(insertTuple) should give an error code\n"){ 
  INITIALIZER(runMissingOperation);
}
TESTCASE("GetValueInUpdate", 
	"Test that it's not possible to perform getValue in an update\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runGetValueInUpdate);
  FINALIZER(runClearTable);
}
TESTCASE("UpdateWithoutKeys", 
	"Test that it's not possible to perform update without setting\n"
	 "PKs"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runUpdateWithoutKeys);
  FINALIZER(runClearTable);
}
TESTCASE("UpdateWithoutValues", 
	"Test that it's not possible to perform update without setValues\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runUpdateWithoutValues);
  FINALIZER(runClearTable);
}
TESTCASE("NdbErrorOperation", 
	 "Test that NdbErrorOperation is properly set"){
  INITIALIZER(runCheckGetNdbErrorOperation);
}
#endif
TESTCASE("InterpretedUKLookup", "") {
  INITIALIZER(runLoadTable);
  INITIALIZER(createPkIndex);
  INITIALIZER(runInterpretedUKLookup);
  INITIALIZER(createPkIndex_Drop);
}
TESTCASE("InterpretedUpdate",
         "Test that one can define and execute an interpreted update "
         "using NdbRecord") {
  INITIALIZER(runLoadTable);
  STEP(runInterpretedUpdate);
  FINALIZER(runCheckData);
  FINALIZER(runClearTable);
}
TESTCASE("InterpretedWrite",
         "Test that one can define and execute an interpreted write"
         "using NdbRecord") {
  TC_PROPERTY("CheckDouble", 1);  // Updates, then Inserts
  INITIALIZER(runLoadTable);
  STEP(runInterpretedWrite);
  FINALIZER(runCheckData);
  FINALIZER(runClearTable);
}
TESTCASE("InterpretedWriteOldApi",
         "Test that one can define and execute an interpreted write using the "
         "old Api") {
  TC_PROPERTY("CheckDouble", 1);  // Updates, then Inserts
  INITIALIZER(runLoadTable);
  STEP(runInterpretedWriteOldApi);
  FINALIZER(runCheckData);
  FINALIZER(runClearTable);
}
TESTCASE("InterpretedWriteOldApiFail",
         "Test an interpreted write using the old Api with a failing "
         "interpreted program") {
  TC_PROPERTY("UpdatesValue", Uint32(0));
  INITIALIZER(runLoadTable);
  STEP(runInterpretedWriteOldApiFail);
  STEP(runCheckUpdatesValue);
  FINALIZER(runClearTable);
}
TESTCASE("InterpretedWriteOldApiSkipProg",
         "Test that one can define and execute an interpreted write using the "
         "old Api with no program") {
  TC_PROPERTY("CheckDouble", 1);  // Updates, then Inserts
  TC_PROPERTY("SkipProgram", 1);  // No program supplied
  INITIALIZER(runLoadTable);
  STEP(runInterpretedWriteOldApi);
  FINALIZER(runCheckData);
  FINALIZER(runClearTable);
}
TESTCASE("InterpretedWriteInsert",
         "Test interpretedWrite program does not run in INSERT case") {
  TC_PROPERTY("AcceptError", 4539);
  INITIALIZER(runLoadTable);
  STEP(runInterpretedWriteInsert);
  FINALIZER(runClearTable);
}
TESTCASE("InterpretedWriteProgram",
         "Test interpreted write with a custom interpreted program similar to "
         "conflict detection interpreted programs") {
  INITIALIZER(runLoadTable);
  STEP(runInterpretedWriteProgram);
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testInterpreter)

int main(int argc, const char **argv) {
  ndb_init();
  //  TABLE("T1");
  NDBT_TESTSUITE_INSTANCE(testInterpreter);
  return testInterpreter.execute(argc, argv);
}
