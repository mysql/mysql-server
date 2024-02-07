/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#ifndef SCAN_INTERPRET_TEST_HPP
#define SCAN_INTERPRET_TEST_HPP

#include "ScanFilter.hpp"

class ScanInterpretTest {
 public:
  ScanInterpretTest(const NdbDictionary::Table &_tab,
                    const NdbDictionary::Table &_restab)
      : tab(_tab), restab(_restab), row(_tab) {}

  int scanRead(Ndb *, int records, int parallelism, ScanFilter &filter);
  int scanReadVerify(Ndb *, int records, int parallelism, ScanFilter &filter);

  int addRowToInsert(Ndb *pNdb, NdbConnection *pInsTrans);
  int addRowToCheckTrans(Ndb *pNdb, NdbConnection *pCheckTrans);

 private:
  const NdbDictionary::Table &tab;
  const NdbDictionary::Table &restab;
  NDBT_ResultRow row;
};

inline int ScanInterpretTest::addRowToInsert(Ndb *pNdb,
                                             NdbConnection *pInsTrans) {
  NdbOperation *pOp = pInsTrans->getNdbOperation(restab.getName());
  if (pOp == NULL) {
    NDB_ERR(pInsTrans->getNdbError());
    pNdb->closeTransaction(pInsTrans);
    return NDBT_FAILED;
  }

  if (pOp->insertTuple() == -1) {
    NDB_ERR(pInsTrans->getNdbError());
    pNdb->closeTransaction(pInsTrans);
    return NDBT_FAILED;
  }

  // Copy all attribute to the new operation
  for (int a = 0; a < restab.getNoOfColumns(); a++) {
    const NdbDictionary::Column *attr = tab.getColumn(a);
    NdbRecAttr *reca = row.attributeStore(a);
    int check = -1;
    switch (attr->getType()) {
      case NdbDictionary::Column::Char:
      case NdbDictionary::Column::Varchar:
      case NdbDictionary::Column::Binary:
      case NdbDictionary::Column::Varbinary: {
        check = pOp->setValue(attr->getName(), reca->aRef());
        break;
      }
      case NdbDictionary::Column::Int: {
        check = pOp->setValue(attr->getName(), reca->int32_value());
      } break;
      case NdbDictionary::Column::Bigint: {
        check = pOp->setValue(attr->getName(), reca->int64_value());
      } break;
      case NdbDictionary::Column::Unsigned: {
        check = pOp->setValue(attr->getName(), reca->u_32_value());
      } break;
      case NdbDictionary::Column::Bigunsigned: {
        check = pOp->setValue(attr->getName(), reca->u_64_value());
      } break;
      case NdbDictionary::Column::Float:
        check = pOp->setValue(attr->getName(), reca->float_value());

        break;
      default:
        check = -1;
        break;
    }
    if (check != 0) {
      NDB_ERR(pInsTrans->getNdbError());
      pNdb->closeTransaction(pInsTrans);
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

inline int ScanInterpretTest::addRowToCheckTrans(Ndb *pNdb,
                                                 NdbConnection *pCheckTrans) {
  NdbOperation *pOp = pCheckTrans->getNdbOperation(restab.getName());
  if (pOp == NULL) {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  if (pOp->readTuple() != 0) {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  // Copy pk attribute's to the new operation
  for (int a = 0; a < restab.getNoOfColumns(); a++) {
    const NdbDictionary::Column *attr = restab.getColumn(a);
    if (attr->getPrimaryKey() == true) {
      NdbRecAttr *reca = row.attributeStore(a);
      int check = -1;
      switch (attr->getType()) {
        case NdbDictionary::Column::Char:
        case NdbDictionary::Column::Varchar:
        case NdbDictionary::Column::Binary:
        case NdbDictionary::Column::Varbinary: {
          check = pOp->equal(attr->getName(), reca->aRef());
          break;
        }
        case NdbDictionary::Column::Int: {
          check = pOp->equal(attr->getName(), reca->int32_value());
        } break;
        case NdbDictionary::Column::Bigint: {
          check = pOp->equal(attr->getName(), reca->int64_value());
        } break;
        case NdbDictionary::Column::Unsigned: {
          check = pOp->equal(attr->getName(), reca->u_32_value());
        } break;
        case NdbDictionary::Column::Bigunsigned: {
          check = pOp->equal(attr->getName(), reca->u_64_value());
        } break;
        default:
          check = -1;
          break;
      }
      if (check != 0) {
        NDB_ERR(pNdb->getNdbError());
        return NDBT_FAILED;
      }
    }
  }

  return NDBT_OK;
}

inline int ScanInterpretTest::scanRead(Ndb *pNdb, int records, int parallelism,
                                       ScanFilter &filter) {
  int retryAttempt = 0;
  int retryMax = 100;
  int check;
  NdbConnection *pTrans;
  NdbScanOperation *pOp;

  while (true) {
    if (retryAttempt >= retryMax) {
      ndbout << "ERROR: has retried this operation " << retryAttempt
             << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();
      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    pOp = pTrans->getNdbScanOperation(tab.getName());
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (pOp->readTuples(NdbScanOperation::LM_Read, 0, parallelism)) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (filter.filterOp(pOp) != 0) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    // Read all attributes
    for (int a = 0; a < tab.getNoOfColumns(); a++) {
      if ((row.attributeStore(a) =
               pOp->getValue(tab.getColumn(a)->getName())) == 0) {
        NDB_ERR(pTrans->getNdbError());
        pNdb->closeTransaction(pTrans);
        return NDBT_FAILED;
      }
    }
    check = pTrans->execute(NoCommit);
    if (check == -1) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    int eof;
    int rows = 0;

    while ((eof = pOp->nextResult(true)) == 0) {
      do {
        rows++;
        if (addRowToInsert(pNdb, pTrans) != 0) {
          pNdb->closeTransaction(pTrans);
          return NDBT_FAILED;
        }
      } while ((eof = pOp->nextResult(false)) == 0);

      check = pTrans->execute(Commit);
      if (check == -1) {
        const NdbError err = pTrans->getNdbError();
        NDB_ERR(err);
        pNdb->closeTransaction(pTrans);
        return NDBT_FAILED;
      }
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        pNdb->closeTransaction(pTrans);
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      NDB_ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    pNdb->closeTransaction(pTrans);

    g_info << rows << " rows have been scanned" << endl;

    return NDBT_OK;
  }
  return NDBT_FAILED;
}

inline int ScanInterpretTest::scanReadVerify(Ndb *pNdb, int records,
                                             int parallelism,
                                             ScanFilter &filter) {
  int retryAttempt = 0;
  const int retryMax = 100;
  int check;
  NdbConnection *pTrans;
  NdbScanOperation *pOp;

  while (true) {
    if (retryAttempt >= retryMax) {
      ndbout << "ERROR: has retried this operation " << retryAttempt
             << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();
      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    pOp = pTrans->getNdbScanOperation(tab.getName());
    if (pOp == NULL || pOp->getValue("KOL2") == NULL) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (pOp->readTuples(NdbScanOperation::LM_Read, 0, parallelism)) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    // Read all attributes
    for (int a = 0; a < tab.getNoOfColumns(); a++) {
      if ((row.attributeStore(a) =
               pOp->getValue(tab.getColumn(a)->getName())) == 0) {
        NDB_ERR(pTrans->getNdbError());
        pNdb->closeTransaction(pTrans);
        return NDBT_FAILED;
      }
    }
    check = pTrans->execute(NoCommit);
    if (check == -1) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    int eof;
    int rows = 0;
    int rowsNoExist = 0;
    int rowsExist = 0;
    int existingRecordsNotFound = 0;
    int nonExistingRecordsFound = 0;

    NdbConnection *pExistTrans;
    NdbConnection *pNoExistTrans;

    while ((eof = pOp->nextResult(true)) == 0) {
      pExistTrans = pNdb->startTransaction();
      if (pExistTrans == NULL) {
        const NdbError err = pNdb->getNdbError();
        NDB_ERR(err);
        return NDBT_FAILED;
      }
      pNoExistTrans = pNdb->startTransaction();
      if (pNoExistTrans == NULL) {
        const NdbError err = pNdb->getNdbError();
        NDB_ERR(err);
        return NDBT_FAILED;
      }
      do {
        rows++;
        if (filter.verifyRecord(row) == NDBT_OK) {
          rowsExist++;
          if (addRowToCheckTrans(pNdb, pExistTrans) != 0) {
            pNdb->closeTransaction(pTrans);
            pNdb->closeTransaction(pExistTrans);
            pNdb->closeTransaction(pNoExistTrans);
            return NDBT_FAILED;
          }
        } else {
          rowsNoExist++;
          if (addRowToCheckTrans(pNdb, pNoExistTrans) != 0) {
            pNdb->closeTransaction(pTrans);
            pNdb->closeTransaction(pExistTrans);
            pNdb->closeTransaction(pNoExistTrans);
            return NDBT_FAILED;
          }
        }
      } while ((eof = pOp->nextResult(false)) == 0);

      // Execute the transaction containing reads of
      // all the records that should be in the result table
      check = pExistTrans->execute(Commit);
      if (check == -1) {
        const NdbError err = pExistTrans->getNdbError();
        NDB_ERR(err);
        if (err.code != 626) {
          pNdb->closeTransaction(pExistTrans);
          pNdb->closeTransaction(pNoExistTrans);
          pNdb->closeTransaction(pTrans);
          return NDBT_FAILED;
        } else {
          // Some of the records expected to be found wasn't
          // there
          existingRecordsNotFound = 1;
        }
      }
      pNdb->closeTransaction(pExistTrans);

      // Execute the transaction containing reads of
      // all the records that should NOT be in the result table
      check = pNoExistTrans->execute(Commit, CommitAsMuchAsPossible);
      if (check == -1) {
        const NdbError err = pNoExistTrans->getNdbError();
        // The transactions error code should be zero
        if (err.code != 626) {
          NDB_ERR(err);
          pNdb->closeTransaction(pNoExistTrans);
          pNdb->closeTransaction(pTrans);
          return NDBT_FAILED;
        }
        // Loop through the no existing transaction and check that no
        // operations where successful
        const NdbOperation *pOp2 = NULL;
        while ((pOp2 = pNoExistTrans->getNextCompletedOperation(pOp2)) !=
               NULL) {
          const NdbError err = pOp2->getNdbError();
          if (err.code != 626) {
            ndbout << "err.code = " << err.code << endl;
            nonExistingRecordsFound = 1;
          }
        }
      }

      pNdb->closeTransaction(pNoExistTrans);
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError) {
        NDB_ERR(err);
        pNdb->closeTransaction(pTrans);
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      NDB_ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    int testResult = NDBT_OK;
    int rowsResult = 0;
    UtilTransactions utilTrans(restab);
    if (utilTrans.selectCount(pNdb, 240, &rowsResult) != 0) {
      return NDBT_FAILED;
    }
    if (existingRecordsNotFound == 1) {
      ndbout << "!!! Expected records not found" << endl;
      testResult = NDBT_FAILED;
    }
    if (nonExistingRecordsFound == 1) {
      ndbout << "!!! Unxpected records found" << endl;
      testResult = NDBT_FAILED;
    }
    ndbout << rows << " rows scanned(" << rowsExist << " found, " << rowsResult
           << " expected)" << endl;
    if (rowsResult != rowsExist) {
      ndbout << "!!! Number of rows in result table different from expected"
             << endl;
      testResult = NDBT_FAILED;
    }

    return testResult;
  }
  return NDBT_FAILED;
}

#endif
