/*
 Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NdbApi.hpp>

#include "NdbQueryBuilder.hpp"
#include "NdbQueryOperation.hpp"

#include <node.h>

#include "adapter_global.h"
#include "unified_debug.h"

#include "QueryOperation.h"
#include "TransactionImpl.h"

enum {
  flag_row_is_null          = 1,
  flag_table_is_join_table  = 2,
  flag_row_is_duplicate     = 8,
};

QueryOperation::QueryOperation(int sz) :
  size(sz),
  buffers(new QueryBuffer[sz]),
  operationTree(0),
  definedQuery(0),
  ndbQuery(0),
  transaction(0),
  results(0),
  latest_error(0),
  nresults(0),
  nheaders(0),
  nextHeaderAllocationSize(1024)
{
  ndbQueryBuilder = NdbQueryBuilder::create();
  DEBUG_PRINT("Size: %d", size);
}

QueryOperation::~QueryOperation() {
  ndbQueryBuilder->destroy();
  delete[] buffers;
  free(results);
}

void QueryOperation::createRowBuffer(int level, Record *record, int parent_table) {
  buffers[level].record = record;
  buffers[level].buffer = new char[record->getBufferSize()];
  buffers[level].size   = record->getBufferSize();
  buffers[level].parent = (short) parent_table;
}

void QueryOperation::levelIsJoinTable(int level) {
  DEBUG_PRINT("Level %d is join table", level);
  buffers[level].static_flags |= flag_table_is_join_table;
}

void QueryOperation::prepare(const NdbQueryOperationDef * root) {
  DEBUG_MARKER(UDEB_DEBUG);
  operationTree = root;
  definedQuery = ndbQueryBuilder->prepare();
}

int QueryOperation::prepareAndExecute() {
  return transaction->prepareAndExecuteQuery(this);
}


/* Check whether this row and its parent are duplicates,
   assuming parent has already been tested and flagged.
   An optimization here would be to scan only the key fields.
*/
bool QueryOperation::isDuplicate(int level) {
  QueryBuffer & current = buffers[level];
  QueryBuffer & parent  = buffers[current.parent];
  char * & result       = current.buffer;
  uint32_t & result_sz  = current.size;
  int lastResult        = current.result;  // most recent result for this table

  /* If the parent is a known duplicate, and the current value matches the
     immediate previous value for this table, then it is a duplicate.
  */
  if((level == 0 || parent.result_flags & flag_row_is_duplicate) &&
      nresults &&                   // this is not the first result for root
      lastResult >= level &&       // and not the first result at this level
     ! (memcmp(results[lastResult].data, result, result_sz)))
  {
    current.result_flags |= flag_row_is_duplicate;
    return true;
  }

  return false;
}

/* takes sector number and two result header indexes
   returns true if results are identical.
*/
bool QueryOperation::compareTwoResults(int level, int r1, int r2) {
  if(r1 == r2) return true;
//  DEBUG_PRINT_DETAIL("compareTwoResults for level %d: %d <=> %d", level, r2, r1);
  assert(level == results[r1].sector);
  assert(level == results[r2].sector);
  return ! memcmp(results[r1].data, results[r2].data, buffers[level].size);
}

/* Takes number of leaf sector number and leaf result header indexes.
   walks to root.  returns true if results are identical at all nodes.
*/
bool QueryOperation::compareFullRows(int level, int r1, int r2) {
  bool didCompareRoot;
  do {
    if(! compareTwoResults(level, r1, r2)) return false;
    didCompareRoot = (level == 0);
    level = buffers[level].parent;
    r1 = results[r1].parent;
    r2 = results[r2].parent;
  } while(! didCompareRoot);

  return true;
}

/* Takes a leaf result header index and returns true if it matches any
   previous row.
*/
bool QueryOperation::compareRowToAllPrevious() {
  int r2 = nresults - 1;           // r2: the latest result
  int r1 = results[r2].previous;   // r1: the earlier result
  int level = results[r2].sector;  // sector
//  DEBUG_PRINT_DETAIL("compareRowToAllPrevious %d %d %d", level, r2, r1);
  while(r1 >= level) {
    assert(r1 < r2);
    if(compareFullRows(level, r1, r2)) {
      return true;
    }
    r1 = results[r1].previous;
  }
  return false;
}


bool QueryOperation::pushResultForTable(short level) {
  QueryBuffer & current = buffers[level];
  QueryBuffer & parent = buffers[current.parent];

  if(level == 0)                            // reset flags for new root result
    for(int i = 0 ; i < this->size ; i++)
      buffers[i].result_flags = buffers[i].static_flags;

  /* Push NULL result, or skip if parent was also NULL */
  if(ndbQuery->getQueryOperation(level)->isRowNULL())
  {
    current.result_flags |= flag_row_is_null;
    if(parent.result_flags & flag_row_is_null)
    {
      DEBUG_PRINT("table %d SKIP -- parent is null", level);
      return true;   /* skip */
    }
    return pushResultNull(level);
  }

  if(isDuplicate(level))
  {
    DEBUG_PRINT("table %d SKIP DUPLICATE", level);
    return true;  /* skip */
  }

  bool ok = pushResultValue(level);

  /* Finally compare the entire row against all previous values,
     unless it is the very first row.
  */
  if(ok && (int) nresults > size) {
    if(compareRowToAllPrevious()) {
      int r = nresults - 1;
      DEBUG_PRINT("table %d PRUNE LAST RESULT", results[r].sector);
      results[r].tag |= flag_row_is_duplicate;
      free(results[r].data);
    }
  }
  return ok;
}

bool QueryOperation::newResultForTable(short level) {
  bool ok = true;
  int n = nresults;

  if(n == nheaders) {
    ok = growHeaderArray();
  }
  if(ok) {
    nresults++;
    results[n].sector = level;
    results[n].previous = buffers[level].result;  // index of previous result
    buffers[level].result = n;                    // index of this result
    results[n].parent = buffers[buffers[level].parent].result;  // current value
    results[n].tag = buffers[level].result_flags;
  }
  return ok;
}

bool QueryOperation::pushResultNull(short level) {
  int n = nresults;
  bool ok = newResultForTable(level);
  if(ok) {
    DEBUG_PRINT("table %d NULL", level);
    results[n].data = 0;
  }
  return ok;
}

bool QueryOperation::pushResultValue(short level) {
  int n = nresults;
  uint32_t & size = buffers[level].size;
  char * & temp_result = buffers[level].buffer;
  bool ok = newResultForTable(level);
  if(ok) {
    DEBUG_PRINT("table %d USE RESULT", level);

    /* Allocate space for the new result */
    results[n].data = (char *) malloc(size);
    if(! results[n].data) return false;

    /* Copy from the holding buffer to the new result */
    memcpy(results[n].data, temp_result, size);
  }
  return ok;
}

QueryResultHeader * QueryOperation::getResult(int id) {
//  DEBUG_PRINT_DETAIL("R %d : TABLE %d TAG %d PARENT %d", id,
//                     results[id].sector, results[id].tag, results[id].parent);
  return (id < nresults) ?  & results[id] : 0;
}

inline bool more(int status) {  /* 0 or 2 */
  return ((status == NdbQuery::NextResult_gotRow) ||
          (status == NdbQuery::NextResult_bufferEmpty));
}

inline bool isError(int status) { /* -1 */
  return (status == NdbQuery::NextResult_error);
}


/* Returns number of results, or an error code < 0
*/
int QueryOperation::fetchAllResults() {
  int status = NdbQuery::NextResult_bufferEmpty;

  while(more(status)) {
    status = ndbQuery->nextResult();
    switch(status) {
      case NdbQuery::NextResult_gotRow:
        /* New results at every level */
        DEBUG_PRINT_DETAIL("NextResult_gotRow");
        for(short level = 0 ; level < size ; level++) {
          if(! pushResultForTable(level)) return -1;
        }
        break;

      case NdbQuery::NextResult_scanComplete:
        DEBUG_PRINT_DETAIL("NextResult_scanComplete");
        break;

      default:
        assert(status == NdbQuery::NextResult_error);
        latest_error = & ndbQuery->getNdbError();
        DEBUG_PRINT("%d %s", latest_error->code, latest_error->message);
        return -1;
    }
  }
  /* All done with the query now. */
  ndbQuery->close();
  ndbQuery = 0;

  return nresults;
}

bool QueryOperation::growHeaderArray() {
  DEBUG_PRINT("growHeaderArray %d => %d", nheaders, nextHeaderAllocationSize);
  QueryResultHeader * old_results = results;

  results = (QueryResultHeader *) calloc(nextHeaderAllocationSize, sizeof(QueryResultHeader));
  if(results) {
    memcpy(results, old_results, nheaders * sizeof(QueryResultHeader));
    free(old_results);
    nheaders = nextHeaderAllocationSize;
    nextHeaderAllocationSize *= 2;
    return true;
  }
  return false; // allocation failed
}

const NdbQueryOperationDef *
  QueryOperation::defineOperation(const NdbDictionary::Index * index,
                                  const NdbDictionary::Table * table,
                                  const NdbQueryOperand* const keys[]) {
  const NdbQueryOperationDef * rval = 0;
  NdbQueryIndexBound * bound;

  if(index) {
    switch(index->getType()) {
      case NdbDictionary::Index::UniqueHashIndex:
        rval = ndbQueryBuilder->readTuple(index, table, keys);
        DEBUG_PRINT("defineOperation using UniqueHashIndex %s", index->getName());
        break;

      case NdbDictionary::Index::OrderedIndex:
        bound = new NdbQueryIndexBound(keys);
        rval = ndbQueryBuilder->scanIndex(index, table, bound);
        DEBUG_PRINT("defineOperation using OrderedIndex %s", index->getName());
        break;
      default:
        DEBUG_PRINT("defineOperation ERROR: default case");
        return 0;
    }
  }
  else {
    rval = ndbQueryBuilder->readTuple(table, keys);
    DEBUG_PRINT("defineOperation using PrimaryKey");
  }

  if(rval == 0) {
    latest_error = & ndbQueryBuilder->getNdbError();
    DEBUG_PRINT("defineOperation: Error %d %s", latest_error->code, latest_error->message);
  }
  return rval;
}

bool QueryOperation::createNdbQuery(NdbTransaction *tx) {
  DEBUG_MARKER(UDEB_DEBUG);
  ndbQuery = tx->createQuery(definedQuery);
  if(! ndbQuery) {
    DEBUG_PRINT("createQuery returned null");
    return false;
  }

  for(int i = 0 ; i < size ; i++) {
    NdbQueryOperation * qop = ndbQuery->getQueryOperation(i);
    if(! qop) {
      DEBUG_PRINT("No Query Operation at index %d", i);
      return false;
    }
    assert(buffers[i].record);
    qop->setResultRowBuf(buffers[i].record->getNdbRecord(), buffers[i].buffer);
  }
  return true;
}

void QueryOperation::setTransactionImpl(TransactionImpl *tx) {
  transaction = tx;
}

void QueryOperation::close() {
  DEBUG_ENTER();
  definedQuery->destroy();
}

const NdbError & QueryOperation::getNdbError() {
  return ndbQueryBuilder->getNdbError();
}
