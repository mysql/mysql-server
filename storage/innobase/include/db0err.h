/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/db0err.h
 Global error codes for the database

 Created 5/24/1996 Heikki Tuuri
 *******************************************************/

#ifndef db0err_h
#define db0err_h

/* Do not include univ.i because univ.i includes this. */

enum dberr_t {
  DB_ERROR_UNSET = 0,
  /** like DB_SUCCESS, but a new explicit record lock was created */
  DB_SUCCESS_LOCKED_REC = 9,
  DB_SUCCESS = 10,

  /* The following are error codes */

  DB_ERROR,
  DB_INTERRUPTED,
  DB_OUT_OF_MEMORY,
  /** The tablespace could not be auto-extending */
  DB_OUT_OF_FILE_SPACE,
  DB_OUT_OF_DISK_SPACE,
  DB_LOCK_WAIT,
  DB_DEADLOCK,
  DB_ROLLBACK,
  DB_DUPLICATE_KEY,
  /** required history data has been deleted due to lack of space in
  rollback segment */
  DB_MISSING_HISTORY,
  /** skip lock */
  DB_SKIP_LOCKED,
  /** don't wait lock */
  DB_LOCK_NOWAIT,
  /** no session temporary tablespace could be allocated */
  DB_NO_SESSION_TEMP,
  DB_CLUSTER_NOT_FOUND = 30,
  DB_TABLE_NOT_FOUND,
  /** the database has to be stopped and restarted with more file space */
  DB_MUST_GET_MORE_FILE_SPACE,
  DB_TABLE_IS_BEING_USED,
  /** a record in an index would not fit on a compressed page, or it would
  become bigger than 1/2 free space in an uncompressed page frame */
  DB_TOO_BIG_RECORD,
  /** lock wait lasted too long */
  DB_LOCK_WAIT_TIMEOUT,
  /** referenced key value not found for a foreign key in an insert or
  update of a row */
  DB_NO_REFERENCED_ROW,
  /** cannot delete or update a row because it contains a key value which
  is referenced */
  DB_ROW_IS_REFERENCED,
  /** adding a foreign key constraint to a table failed */
  DB_CANNOT_ADD_CONSTRAINT,
  /** data structure corruption noticed */
  DB_CORRUPTION,
  /** dropping a foreign key constraint from a table failed */
  DB_CANNOT_DROP_CONSTRAINT,
  /** no savepoint exists with the given name */
  DB_NO_SAVEPOINT,
  /** cannot create a new tablespace because a file of the same name or
  tablespace ID already exists */
  DB_TABLESPACE_EXISTS,
  /** tablespace was deleted or is being dropped right now */
  DB_TABLESPACE_DELETED,
  /** Attempt to delete a tablespace instance that was not found in the
  tablespace hash table */
  DB_TABLESPACE_NOT_FOUND,
  /** lock structs have exhausted the buffer pool (for big transactions,
  InnoDB stores the lock structs in the buffer pool) */
  DB_LOCK_TABLE_FULL,
  /** foreign key constraints activated by the operation would lead to a
  duplicate key in some table */
  DB_FOREIGN_DUPLICATE_KEY,
  /** when InnoDB runs out of the preconfigured undo slots, this can only
  happen when there are too many concurrent transactions */
  DB_TOO_MANY_CONCURRENT_TRXS,
  /** when InnoDB sees any artefact or a feature that it can't recoginize
  or work with e.g., FT indexes created by a later version of the engine. */
  DB_UNSUPPORTED,
  /** a NOT NULL column was found to be NULL during table rebuild */
  DB_INVALID_NULL,
  /** an operation that requires the persistent storage, used for recording
  table and index statistics, was requested but this storage does not exist
  itself or the stats for a given table do not exist */
  DB_STATS_DO_NOT_EXIST,
  /** Foreign key constraint related cascading delete/update exceeds maximum
  allowed depth */
  DB_FOREIGN_EXCEED_MAX_CASCADE,
  /** the child (foreign) table does not have an index that contains the
  foreign keys as its prefix columns */
  DB_CHILD_NO_INDEX,
  /** the parent table does not have an index that contains the foreign keys
  as its prefix columns */
  DB_PARENT_NO_INDEX,
  /** index column size exceeds maximum limit */
  DB_TOO_BIG_INDEX_COL,
  /** we have corrupted index */
  DB_INDEX_CORRUPT,
  /** the undo log record is too big */
  DB_UNDO_RECORD_TOO_BIG,
  /** Update operation attempted in a read-only transaction */
  DB_READ_ONLY,
  /** FTS Doc ID cannot be zero */
  DB_FTS_INVALID_DOCID,
  /** table is being used in foreign key check */
  DB_TABLE_IN_FK_CHECK,
  /** Modification log grew too big during online index creation */
  DB_ONLINE_LOG_TOO_BIG,
  /** Identifier name too long */
  DB_IDENTIFIER_TOO_LONG,
  /** FTS query memory exceeds result cache limit */
  DB_FTS_EXCEED_RESULT_CACHE_LIMIT,
  /** Temp file write failure */
  DB_TEMP_FILE_WRITE_FAIL,
  /** Cannot create specified Geometry data object */
  DB_CANT_CREATE_GEOMETRY_OBJECT,
  /** Cannot open a file */
  DB_CANNOT_OPEN_FILE,
  /** Too many words in a phrase */
  DB_FTS_TOO_MANY_WORDS_IN_PHRASE,
  /** Server version is lower than tablespace version */
  DB_SERVER_VERSION_LOW,
  /** The path is too long for the OS */
  DB_TOO_LONG_PATH,
  /** Generic IO error */
  DB_IO_ERROR = 100,
  /** Failure to decompress a page after reading it from disk */
  DB_IO_DECOMPRESS_FAIL,
  /** Punch hole not supported by InnoDB */
  DB_IO_NO_PUNCH_HOLE,
  /** The file system doesn't support punch hole */
  DB_IO_NO_PUNCH_HOLE_FS,
  /** The tablespace doesn't support punch hole */
  DB_IO_NO_PUNCH_HOLE_TABLESPACE,
  /** Failure to decrypt a page after reading it from disk */
  DB_IO_DECRYPT_FAIL,
  /** The tablespace doesn't support encrypt */
  DB_IO_NO_ENCRYPT_TABLESPACE,
  /** Partial IO request failed */
  DB_IO_PARTIAL_FAILED,
  /** Transaction was forced to rollback by a higher priority transaction */
  DB_FORCED_ABORT,
  /** Table/clustered index is corrupted */
  DB_TABLE_CORRUPT,
  /** Invalid Filename */
  DB_WRONG_FILE_NAME,
  /** Compute generated value failed */
  DB_COMPUTE_VALUE_FAILED,
  /** Cannot add foreign constrain placed on the base column of stored column */
  DB_NO_FK_ON_S_BASE_COL,
  /** Invalid encryption metadata in page 0. */
  DB_INVALID_ENCRYPTION_META,
  /** Incomplete cloned directory */
  DB_ABORT_INCOMPLETE_CLONE,
  /** Btree level limit exceeded. */
  DB_BTREE_LEVEL_LIMIT_EXCEEDED,
  /** Doublewrite meta data not found in the system space header */
  DB_DBLWR_NOT_EXISTS,
  /** Failed to initialize the doublewrite extents in the system tablespace */
  DB_V1_DBLWR_INIT_FAILED,
  /** Failed to create the doublewrite extents in the system tablespace */
  DB_V1_DBLWR_CREATE_FAILED,
  /** Failed to initialize the double write memory data structures */
  DB_DBLWR_INIT_FAILED,
  /* Schema mismatch between the metadata and data being imported. */
  DB_SCHEMA_MISMATCH,
  /** System has run out of resources. */
  DB_OUT_OF_RESOURCES,
  /** Page was discarded, was not written to storage. */
  DB_PAGE_IS_STALE,
  /** Error reading the auto-increment value. */
  DB_AUTOINC_READ_ERROR,
  /** Failed to read as read was beyond file size. */
  DB_FILE_READ_BEYOND_SIZE,

  /* The following are partial failure codes */

  DB_FAIL = 1000,
  DB_OVERFLOW,
  DB_UNDERFLOW,
  DB_STRONG_FAIL,
  DB_ZIP_OVERFLOW,
  DB_RECORD_NOT_FOUND = 1500,
  DB_END_OF_BLOCK,
  DB_END_OF_INDEX,
  DB_END_SAMPLE_READ,

  /** Generic error code for "Not found" type of errors */
  DB_NOT_FOUND,

  /* The following are API only error codes. */

  /** Column update or read failed because the types mismatch */
  DB_DATA_MISMATCH = 2000,
  /* Too many nested sub expression in full-text search string */
  DB_FTS_TOO_MANY_NESTED_EXP
};
#endif
