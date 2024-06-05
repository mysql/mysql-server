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

#ifndef HUGO_ASYNCH_TRANSACTIONS_HPP
#define HUGO_ASYNCH_TRANSACTIONS_HPP

#include <HugoCalculator.hpp>
#include <HugoTransactions.hpp>
#include <NDBT.hpp>

class HugoAsynchTransactions : private HugoTransactions {
 public:
  HugoAsynchTransactions(const NdbDictionary::Table &);
  ~HugoAsynchTransactions();
  int loadTableAsynch(Ndb *, int records = 0, int batch = 1, int trans = 1,
                      int operations = 1);
  int pkReadRecordsAsynch(Ndb *, int records = 0, int batch = 1, int trans = 1,
                          int operations = 1);
  int pkUpdateRecordsAsynch(Ndb *, int records = 0, int batch = 1,
                            int trans = 1, int operations = 1);
  int pkDelRecordsAsynch(Ndb *, int records = 0, int batch = 1, int trans = 1,
                         int operations = 1);

 private:
  enum NDB_OPERATION { NO_INSERT, NO_UPDATE, NO_READ, NO_DELETE };

  long transactionsCompleted;

  struct TransactionInfo {
    HugoAsynchTransactions *hugoP;
    NdbConnection *transaction;
    int startRecordId;
    int numRecords;
    int resultRowStartIndex;
    int retries;
    NDB_OPERATION opType;
  };

  TransactionInfo *transInfo;
  Ndb *theNdb;

  /* Work description */
  int totalLoops;
  int recordsPerLoop;
  int maxOpsPerTrans;
  NDB_OPERATION operationType;
  ExecType execType;

  /* Progress description */
  int nextUnProcessedRecord;
  int loopNum;
  int totalCompletedRecords;
  int maxUsedRetries;
  bool finished;
  int testResult;

  void allocTransactions(int trans, int maxOpsPerTrans);
  void deallocTransactions();

  int getNextWorkTask(int *startRecordId, int *numRecords);

  int defineUpdateOpsForTask(TransactionInfo *tInfo);
  int defineTransactionForTask(TransactionInfo *tInfo, ExecType taskExecType);

  int beginNewTask(TransactionInfo *tInfo);
  static void callbackFunc(int result, NdbConnection *trans, void *anObject);
  void callback(int result, NdbConnection *trans, TransactionInfo *tInfo);

  int executeAsynchOperation(Ndb *, int records, int batch, int trans,
                             int operations, NDB_OPERATION theOperation,
                             ExecType theType = Commit);
};

#endif
