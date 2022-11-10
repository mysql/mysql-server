/*
   Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

// Implements the interface defined in
#include "storage/ndb/plugin/ndb_table_stats.h"

// Using
#include "my_dbug.h"
#include "storage/ndb/include/ndbapi/NdbInterpretedCode.hpp"
#include "storage/ndb/include/ndbapi/NdbTransaction.hpp"
#include "storage/ndb/plugin/ndb_sleep.h"
#include "storage/ndb/plugin/ndb_thd.h"

// Empty mask for reading no attributes using NdbRecord, will be initialized
// to all zeros by linker.
static unsigned char empty_mask[(NDB_MAX_ATTRIBUTES_IN_TABLE + 7) / 8];

bool ndb_get_table_statistics(THD *thd, Ndb *ndb,
                              const NdbDictionary::Table *ndbtab,
                              Ndb_table_stats *stats, NdbError &ndb_error,
                              Uint32 part_id) {
  DBUG_TRACE;

  Uint64 rows, fixed_mem, var_mem, ext_space, free_ext_space;
  Uint32 size, fragid;
  NdbOperation::GetValueSpec extraGets[7];
  extraGets[0].column = NdbDictionary::Column::ROW_COUNT;
  extraGets[0].appStorage = &rows;
  extraGets[1].column = NdbDictionary::Column::ROW_SIZE;
  extraGets[1].appStorage = &size;
  extraGets[2].column = NdbDictionary::Column::FRAGMENT_FIXED_MEMORY;
  extraGets[2].appStorage = &fixed_mem;
  extraGets[3].column = NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY;
  extraGets[3].appStorage = &var_mem;
  extraGets[4].column = NdbDictionary::Column::FRAGMENT_EXTENT_SPACE;
  extraGets[4].appStorage = &ext_space;
  extraGets[5].column = NdbDictionary::Column::FRAGMENT_FREE_EXTENT_SPACE;
  extraGets[5].appStorage = &free_ext_space;
  extraGets[6].column = NdbDictionary::Column::FRAGMENT;
  extraGets[6].appStorage = &fragid;

  const Uint32 codeWords = 1;
  Uint32 codeSpace[codeWords];
  NdbInterpretedCode code(nullptr,  // Table is irrelevant
                          &codeSpace[0], codeWords);
  if (code.interpret_exit_last_row() != 0 || code.finalise() != 0) {
    ndb_error = code.getNdbError();  // Return NDB error to caller
    return true;                     // Error
  }

  int retries = 100;
  NdbTransaction *trans;
  do {
    Uint32 count [[maybe_unused]] = 0;
    Uint64 sum_rows = 0;
    Uint64 sum_row_size = 0;
    Uint64 sum_mem = 0;
    Uint64 sum_ext_space = 0;
    Uint64 sum_free_ext_space = 0;
    NdbScanOperation *pOp;
    int check;

    if ((trans = ndb->startTransaction(ndbtab)) == nullptr) {
      ndb_error = ndb->getNdbError();
      goto retry;
    }

    NdbScanOperation::ScanOptions options;
    options.optionsPresent = NdbScanOperation::ScanOptions::SO_BATCH |
                             NdbScanOperation::ScanOptions::SO_GETVALUE |
                             NdbScanOperation::ScanOptions::SO_INTERPRETED;
    /* Set batch=1, as we need only one row per fragment. */
    options.batch = 1;
    options.extraGetValues = &extraGets[0];
    options.numExtraGetValues = sizeof(extraGets) / sizeof(extraGets[0]);
    options.interpretedCode = &code;

    if ((pOp = trans->scanTable(
             ndbtab->getDefaultRecord(),  // Record not used since there are no
                                          // real columns in the scan
             NdbOperation::LM_CommittedRead, empty_mask, &options,
             sizeof(NdbScanOperation::ScanOptions))) == nullptr) {
      ndb_error = trans->getNdbError();
      goto retry;
    }

    if (trans->execute(NdbTransaction::NoCommit, NdbOperation::AbortOnError,
                       1 /* force send */) == -1) {
      ndb_error = trans->getNdbError();
      goto retry;
    }

    const char *dummyRowPtr;
    while ((check = pOp->nextResult(&dummyRowPtr, true, true)) == 0) {
      DBUG_PRINT("info",
                 ("nextResult rows: %llu, "
                  "fixed_mem_size %llu var_mem_size %llu "
                  "fragmentid %u extent_space %llu free_extent_space %llu",
                  rows, fixed_mem, var_mem, fragid, ext_space, free_ext_space));

      if ((part_id != ~(Uint32)0) && fragid != part_id) {
        // Only count fragment with given part_id
        continue;
      }

      sum_rows += rows;
      if (sum_row_size < size) sum_row_size = size;
      sum_mem += fixed_mem + var_mem;
      count++;
      sum_ext_space += ext_space;
      sum_free_ext_space += free_ext_space;

      if ((part_id != ~(Uint32)0) && fragid == part_id) {
        // Found fragment with given part_id, nothing more to do
        break;
      }
    }

    if (check == -1) {
      ndb_error = pOp->getNdbError();
      goto retry;
    }

    pOp->close(true);

    ndb->closeTransaction(trans);

    stats->row_count = sum_rows;
    stats->row_size = sum_row_size;
    stats->fragment_memory = sum_mem;
    stats->fragment_extent_space = sum_ext_space;
    stats->fragment_extent_free_space = sum_free_ext_space;

    DBUG_PRINT("exit", ("records: %llu row_size: %llu "
                        "mem: %llu allocated: %llu free: %llu count: %u",
                        sum_rows, sum_row_size, sum_mem, sum_ext_space,
                        sum_free_ext_space, count));

    return false;  // Success

  retry:

    if (trans != nullptr) {
      ndb->closeTransaction(trans);
      trans = nullptr;
    }
    if (ndb_error.status == NdbError::TemporaryError && retries-- &&
        !thd_killed(thd)) {
      ndb_trans_retry_sleep();
      continue;
    }
    break;
  } while (true);

  DBUG_PRINT("error", ("NDB: %u - %s", ndb_error.code, ndb_error.message));
  return true;  // Error
}

bool ndb_get_table_commit_count(Ndb *ndb, const NdbDictionary::Table *ndbtab,
                                NdbError &ndb_error, Uint64 *commit_count) {
  DBUG_TRACE;

  Uint64 fragment_commit_count;
  NdbOperation::GetValueSpec extraGets[1];
  extraGets[0].column = NdbDictionary::Column::COMMIT_COUNT;
  extraGets[0].appStorage = &fragment_commit_count;

  const Uint32 codeWords = 1;
  Uint32 codeSpace[codeWords];
  NdbInterpretedCode code(nullptr,  // Table is irrelevant
                          &codeSpace[0], codeWords);
  if (code.interpret_exit_last_row() != 0 || code.finalise() != 0) {
    ndb_error = code.getNdbError();
    DBUG_PRINT("error", ("NDB: %u - %s", ndb_error.code, ndb_error.message));
    return true;  // Error
  }

  int retries = 100;
  NdbTransaction *trans;
  do {
    /**
     * Allocate an isolated Ndb object for this scan due to bug#34768887
     * Will be released on leaving scope / iterating loop
     */
    Ndb isolNdb(&ndb->get_ndb_cluster_connection());
    if (isolNdb.init() != 0) {
      DBUG_PRINT("info", ("Failed to init Ndb object : %u",
                          isolNdb.getNdbError().code));
      ndb_error = isolNdb.getNdbError();
      return true;  // Error
    }

    Uint64 total_commit_count = 0;
    NdbScanOperation *op;
    int check;

    if ((trans = isolNdb.startTransaction(ndbtab)) == nullptr) {
      ndb_error = isolNdb.getNdbError();
      goto retry;
    }

    NdbScanOperation::ScanOptions options;
    options.optionsPresent = NdbScanOperation::ScanOptions::SO_BATCH |
                             NdbScanOperation::ScanOptions::SO_GETVALUE |
                             NdbScanOperation::ScanOptions::SO_INTERPRETED;
    /* Set batch=1, as we need only one row per fragment. */
    options.batch = 1;
    options.extraGetValues = &extraGets[0];
    options.numExtraGetValues = sizeof(extraGets) / sizeof(extraGets[0]);
    options.interpretedCode = &code;

    // Read only pseudo columns by scanning with an empty mask and one
    // extra gets
    if ((op = trans->scanTable(
             ndbtab->getDefaultRecord(),  // Record not used since there are no
                                          // real columns in the scan
             NdbOperation::LM_Read,       // LM_Read to control which replicas
             empty_mask, &options, sizeof(NdbScanOperation::ScanOptions))) ==
        nullptr) {
      ndb_error = trans->getNdbError();
      goto retry;
    }

    if (trans->execute(NdbTransaction::NoCommit, NdbOperation::AbortOnError,
                       1 /* force send */) == -1) {
      ndb_error = trans->getNdbError();
      goto retry;
    }

    const char *dummyRowPtr;
    while ((check = op->nextResult(&dummyRowPtr, true, true)) == 0) {
      DBUG_PRINT("info",
                 ("fragment_commit_count: %llu", fragment_commit_count));
      total_commit_count += fragment_commit_count;
    }
    if (check == -1) {
      ndb_error = op->getNdbError();
      goto retry;
    }
    op->close(true);
    isolNdb.closeTransaction(trans);
    *commit_count = total_commit_count;
    DBUG_PRINT("info",
               ("Returning false with commit count %llu", total_commit_count));
    return false;  // Success

  retry:
    DBUG_PRINT("info", ("Temp error : %u", ndb_error.code));
    if (trans != nullptr) {
      isolNdb.closeTransaction(trans);
      trans = nullptr;
    }
    if (ndb_error.status == NdbError::TemporaryError && retries--) {
      ndb_retry_sleep(30);  // milliseconds
      continue;
    }
    break;
  } while (true);

  DBUG_PRINT("error", ("NDB: %u - %s", ndb_error.code, ndb_error.message));
  return true;  // Error
}
