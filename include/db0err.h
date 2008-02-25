/******************************************************
Global error codes for the database

(c) 1996 Innobase Oy

Created 5/24/1996 Heikki Tuuri
*******************************************************/

#ifndef db0err_h
#define db0err_h


enum db_err {
	DB_SUCCESS = 10,

	/* The following are error codes */
	DB_ERROR,
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
	DB_COL_APPEARS_TWICE_IN_INDEX,	/* InnoDB cannot handle an index
					where same column appears twice */
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
	DB_TABLE_ZIP_NO_IBD,		/* trying to create a compressed
					table in the system tablespace */

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
