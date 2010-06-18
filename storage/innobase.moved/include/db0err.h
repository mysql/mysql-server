/******************************************************
Global error codes for the database

(c) 1996 Innobase Oy

Created 5/24/1996 Heikki Tuuri
*******************************************************/

#ifndef db0err_h
#define db0err_h


#define DB_SUCCESS		10

/* The following are error codes */
#define	DB_ERROR		11
#define DB_OUT_OF_MEMORY	12
#define DB_OUT_OF_FILE_SPACE	13
#define DB_LOCK_WAIT		14
#define DB_DEADLOCK		15
#define DB_ROLLBACK		16
#define DB_DUPLICATE_KEY	17
#define DB_QUE_THR_SUSPENDED	18
#define DB_MISSING_HISTORY	19	/* required history data has been
					deleted due to lack of space in
					rollback segment */
#define DB_CLUSTER_NOT_FOUND	30
#define DB_TABLE_NOT_FOUND	31
#define DB_MUST_GET_MORE_FILE_SPACE 32	/* the database has to be stopped
					and restarted with more file space */
#define DB_TABLE_IS_BEING_USED	33
#define DB_TOO_BIG_RECORD	34	/* a record in an index would become
					bigger than 1/2 free space in a page
					frame */
#define DB_LOCK_WAIT_TIMEOUT	35	/* lock wait lasted too long */
#define DB_NO_REFERENCED_ROW	36	/* referenced key value not found
					for a foreign key in an insert or
					update of a row */
#define DB_ROW_IS_REFERENCED	37	/* cannot delete or update a row
					because it contains a key value
					which is referenced */
#define DB_CANNOT_ADD_CONSTRAINT 38	/* adding a foreign key constraint
					to a table failed */
#define DB_CORRUPTION		39	/* data structure corruption noticed */
#define DB_COL_APPEARS_TWICE_IN_INDEX 40/* InnoDB cannot handle an index
					where same column appears twice */
#define DB_CANNOT_DROP_CONSTRAINT 41	/* dropping a foreign key constraint
					from a table failed */
#define DB_NO_SAVEPOINT		42	/* no savepoint exists with the given
					name */
#define	DB_TABLESPACE_ALREADY_EXISTS 43 /* we cannot create a new single-table
					tablespace because a file of the same
					name already exists */
#define DB_TABLESPACE_DELETED	44	/* tablespace does not exist or is
					being dropped right now */
#define	DB_LOCK_TABLE_FULL	45	/* lock structs have exhausted the
					buffer pool (for big transactions,
					InnoDB stores the lock structs in the
					buffer pool) */
#define DB_FOREIGN_DUPLICATE_KEY 46	/* foreign key constraints
					activated by the operation would
					lead to a duplicate key in some
					table */
#define DB_TOO_MANY_CONCURRENT_TRXS 47	/* when InnoDB runs out of the
					preconfigured undo slots, this can
					only happen when there are too many
					concurrent transactions */
#define DB_UNSUPPORTED		48	/* when InnoDB sees any artefact or
					a feature that it can't recoginize or
					work with e.g., FT indexes created by
					a later version of the engine. */
/* The following are partial failure codes */
#define DB_FAIL			1000
#define DB_OVERFLOW		1001
#define DB_UNDERFLOW		1002
#define DB_STRONG_FAIL		1003
#define DB_RECORD_NOT_FOUND	1500
#define DB_END_OF_INDEX		1501

#endif
