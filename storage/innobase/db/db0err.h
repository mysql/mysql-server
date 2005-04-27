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
					and restrated with more file space */
#define DB_TABLE_IS_BEING_USED	33
#define DB_TOO_BIG_RECORD	34	/* a record in an index would become
					bigger than 1/2 free space in a page
					frame */
					
/* The following are partial failure codes */
#define DB_FAIL 		1000
#define DB_OVERFLOW 		1001
#define DB_UNDERFLOW 		1002
#define DB_STRONG_FAIL		1003
#define DB_RECORD_NOT_FOUND	1500
#define DB_END_OF_INDEX		1501

#endif 
