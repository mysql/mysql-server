/*
 Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#ifndef NODEJS_ADAPTER_NDB_INCLUDE_QUERYOPERATION_H
#define NODEJS_ADAPTER_NDB_INCLUDE_QUERYOPERATION_H

#include "KeyOperation.h"

class NdbQueryBuilder;
class NdbQueryOperationDef;
class NdbQueryDef;
class TransactionImpl;
class NdbQueryOperand;
class SessionImpl;

class QueryBuffer {
 public:
  /* Set at initialization time: */
  Record *record;
  char *buffer;
  uint32_t size;  // size of buffer
  short parent;   // index of parent in all QueryBuffers
  uint16_t static_flags;
  /* Used in result construction: */
  uint16_t result_flags;
  uint32_t result;  // index of current result in all ResultHeaders
  QueryBuffer()
      : record(0),
        buffer(0),
        size(0),
        parent(0),
        static_flags(0),
        result_flags(0),
        result(0) {}
  ~QueryBuffer() {
    if (size) delete[] buffer;
  }
};

class QueryResultHeader {
 public:
  char *data;
  uint32_t parent;    // index of current ResultHeader for parent sector
  uint32_t previous;  // index of previous ResultHeader for this sector
  uint16_t sector;
  uint16_t tag;
};

class QueryOperation {
 public:
  QueryOperation(int);
  ~QueryOperation();
  void createRowBuffer(int level, Record *, int parent);
  void levelIsJoinTable(int level);
  int prepareAndExecute();
  void setTransactionImpl(TransactionImpl *);
  bool createNdbQuery(NdbTransaction *);
  void prepare(const NdbQueryOperationDef *, const SessionImpl *);
  int fetchAllResults();
  NdbQueryBuilder *getBuilder() { return ndbQueryBuilder; }
  const NdbQueryOperationDef *defineOperation(
      const NdbDictionary::Index *index, const NdbDictionary::Table *table,
      const NdbQueryOperand *const keys[]);
  QueryResultHeader *getResult(int);
  uint32_t getResultRowSize(int depth);
  void close();
  const NdbError &getNdbError();

 protected:
  bool growHeaderArray();
  bool pushResultValue(short);
  bool pushResultNull(short);
  bool pushResultForTable(short);
  bool newResultForTable(short);
  bool compareTwoResults(int, int, int);
  bool compareFullRows(int, int, int);
  bool isDuplicate(int);
  bool compareRowToAllPrevious();

 private:
  int size;
  QueryBuffer *const buffers;
  NdbQueryBuilder *ndbQueryBuilder;
  const NdbQueryOperationDef *operationTree;
  const NdbQueryDef *definedQuery;
  NdbQuery *ndbQuery;
  TransactionImpl *transaction;
  QueryResultHeader *results;
  const NdbError *latest_error;
  int nresults, nheaders;
  uint32_t nextHeaderAllocationSize;
};

inline uint32_t QueryOperation::getResultRowSize(int depth) {
  return buffers[depth].size;
}

#endif
