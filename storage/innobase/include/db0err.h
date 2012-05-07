/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/db0err.h
Global error codes for the database

Created 5/24/1996 Heikki Tuuri
*******************************************************/

#ifndef db0err_h
#define db0err_h


enum db_err {
	DB_SUCCESS_LOCKED_REC = 9,	/*!< like DB_SUCCESS, but a new
					explicit record lock was created */
	DB_SUCCESS = 10,

	/* The following are error codes */
	DB_ERROR,
	DB_INTERRUPTED,
	DB_OUT_OF_MEMORY,
	DB_OUT_OF_FILE_SPACE,
	DB_LOCK_WAIT,
	DB_DEADLOCK,
	DB_ROLLBACK,
	DB_DUPLICATE_KEY,
	DB_QUE_THR_SUSPENDED,
	DB_MISSING_HISTORY,		/* required history data has been
					deleted due to lack of space in
					rollback segment */
	DB_CLUSTER_NOT_FOUND = 30,
	DB_TABLE_NOT_FOUND,
	DB_MUST_GET_MORE_FILE_SPACE,	/* the database has to be stopped
					and restarted with more file space */
	DB_TABLE_IS_BEING_USED,
	DB_TOO_BIG_RECORD,		/* a record in an index would not fit
					on a compressed page, or it would
					become bigger than 1/2 free space in
					an uncompressed page frame */
	DB_LOCK_WAIT_TIMEOUT,		/* lock wait lasted too long */
	DB_NO_REFERENCED_ROW,		/* referenced key value not found
					for a foreign key in an insert or
					update of a row */
	DB_ROW_IS_REFERENCED,		/* cannot delete or update a row
					because it contains a key value
					which is referenced */
	DB_CANNOT_ADD_CONSTRAINT,	/* adding a foreign key constraint
					to a table failed */
	DB_CORRUPTION,			/* data structure corruption noticed */
	DB_CANNOT_DROP_CONSTRAINT,	/* dropping a foreign key constraint
					from a table failed */
	DB_NO_SAVEPOINT,		/* no savepoint exists with the given
					name */
	DB_TABLESPACE_ALREADY_EXISTS,	/* we cannot create a new single-table
					tablespace because a file of the same
					name already exists */
	DB_TABLESPACE_DELETED,		/* tablespace does not exist or is
					being dropped right now */
	DB_LOCK_TABLE_FULL,		/* lock structs have exhausted the
					buffer pool (for big transactions,
					InnoDB stores the lock structs in the
					buffer pool) */
	DB_FOREIGN_DUPLICATE_KEY,	/* foreign key constraints
					activated by the operation would
					lead to a duplicate key in some
					table */
	DB_TOO_MANY_CONCURRENT_TRXS,	/* when InnoDB runs out of the
					preconfigured undo slots, this can
					only happen when there are too many
					concurrent transactions */
	DB_UNSUPPORTED,			/* when InnoDB sees any artefact or
					a feature that it can't recoginize or
					work with e.g., FT indexes created by
					a later version of the engine. */

	DB_PRIMARY_KEY_IS_NULL,		/* a column in the PRIMARY KEY
					was found to be NULL */

	DB_STATS_DO_NOT_EXIST,		/* an operation that requires the
					persistent storage, used for recording
					table and index statistics, was
					requested but this storage does not
					exist itself or the stats for a given
					table do not exist */
	DB_FOREIGN_EXCEED_MAX_CASCADE,	/* Foreign key constraint related
					cascading delete/update exceeds
					maximum allowed depth */
	DB_CHILD_NO_INDEX,		/* the child (foreign) table does not
					have an index that contains the
					foreign keys as its prefix columns */
	DB_PARENT_NO_INDEX,		/* the parent table does not
					have an index that contains the
					foreign keys as its prefix columns */
	DB_TOO_BIG_INDEX_COL,		/* index column size exceeds maximum
					limit */
	DB_INDEX_CORRUPT,		/* we have corrupted index */
	DB_UNDO_RECORD_TOO_BIG,		/* the undo log record is too big */
	DB_TABLE_IN_FK_CHECK,		/* table is being used in foreign
					key check */

	/* The following are partial failure codes */
	DB_FAIL = 1000,
	DB_OVERFLOW,
	DB_UNDERFLOW,
	DB_STRONG_FAIL,
	DB_ZIP_OVERFLOW,
	DB_RECORD_NOT_FOUND = 1500,
	DB_END_OF_INDEX
};

#endif
