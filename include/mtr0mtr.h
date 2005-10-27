/******************************************************
Mini-transaction buffer

(c) 1995 Innobase Oy

Created 11/26/1995 Heikki Tuuri
*******************************************************/

#ifndef mtr0mtr_h
#define mtr0mtr_h

#include "univ.i"
#include "mem0mem.h"
#include "dyn0dyn.h"
#include "buf0types.h"
#include "sync0rw.h"
#include "ut0byte.h"
#include "mtr0types.h"
#include "page0types.h"

/* Logging modes for a mini-transaction */
#define MTR_LOG_ALL		21	/* default mode: log all operations
					modifying disk-based data */
#define	MTR_LOG_NONE		22	/* log no operations */
/*#define	MTR_LOG_SPACE	23 */	/* log only operations modifying
					file space page allocation data
					(operations in fsp0fsp.* ) */
#define	MTR_LOG_SHORT_INSERTS	24	/* inserts are logged in a shorter
					form */
					
/* Types for the mlock objects to store in the mtr memo; NOTE that the
first 3 values must be RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH */
#define	MTR_MEMO_PAGE_S_FIX	RW_S_LATCH
#define	MTR_MEMO_PAGE_X_FIX	RW_X_LATCH
#define	MTR_MEMO_BUF_FIX	RW_NO_LATCH
#define MTR_MEMO_MODIFY		54
#define	MTR_MEMO_S_LOCK		55
#define	MTR_MEMO_X_LOCK		56

/* Log item types: we have made them to be of the type 'byte'
for the compiler to warn if val and type parameters are switched
in a call to mlog_write_ulint. NOTE! For 1 - 8 bytes, the
flag value must give the length also! */
#define	MLOG_SINGLE_REC_FLAG	128		/* if the mtr contains only
						one log record for one page,
						i.e., write_initial_log_record
						has been called only once,
						this flag is ORed to the type
						of that first log record */
#define	MLOG_1BYTE		((byte)1) 	/* one byte is written */
#define	MLOG_2BYTES		((byte)2)	/* 2 bytes ... */
#define	MLOG_4BYTES		((byte)4)	/* 4 bytes ... */
#define	MLOG_8BYTES		((byte)8)	/* 8 bytes ... */
#define	MLOG_REC_INSERT		((byte)9)	/* record insert */
#define	MLOG_REC_CLUST_DELETE_MARK ((byte)10) 	/* mark clustered index record
						deleted */
#define	MLOG_REC_SEC_DELETE_MARK ((byte)11) 	/* mark secondary index record
						deleted */
#define MLOG_REC_UPDATE_IN_PLACE ((byte)13)	/* update of a record,
						preserves record field sizes */
#define MLOG_REC_DELETE		((byte)14)	/* delete a record from a
						page */
#define	MLOG_LIST_END_DELETE 	((byte)15)	/* delete record list end on
						index page */
#define	MLOG_LIST_START_DELETE 	((byte)16) 	/* delete record list start on
						index page */
#define	MLOG_LIST_END_COPY_CREATED ((byte)17) 	/* copy record list end to a
						new created index page */
#define	MLOG_PAGE_REORGANIZE 	((byte)18)	/* reorganize an index page */
#define MLOG_PAGE_CREATE 	((byte)19)	/* create an index page */
#define	MLOG_UNDO_INSERT 	((byte)20)	/* insert entry in an undo
						log */
#define MLOG_UNDO_ERASE_END	((byte)21)	/* erase an undo log page end */
#define	MLOG_UNDO_INIT 		((byte)22)	/* initialize a page in an
						undo log */
#define MLOG_UNDO_HDR_DISCARD	((byte)23)	/* discard an update undo log
						header */
#define	MLOG_UNDO_HDR_REUSE	((byte)24)	/* reuse an insert undo log
						header */
#define MLOG_UNDO_HDR_CREATE	((byte)25)	/* create an undo log header */
#define MLOG_REC_MIN_MARK	((byte)26)	/* mark an index record as the
						predefined minimum record */
#define MLOG_IBUF_BITMAP_INIT	((byte)27)	/* initialize an ibuf bitmap
						page */
/*#define	MLOG_FULL_PAGE	((byte)28)	full contents of a page */
#define MLOG_INIT_FILE_PAGE	((byte)29)	/* this means that a file page
						is taken into use and the prior
						contents of the page should be
						ignored: in recovery we must
						not trust the lsn values stored
						to the file page */
#define MLOG_WRITE_STRING	((byte)30)	/* write a string to a page */
#define	MLOG_MULTI_REC_END	((byte)31)	/* if a single mtr writes
						log records for several pages,
						this log record ends the
						sequence of these records */
#define MLOG_DUMMY_RECORD	((byte)32)	/* dummy log record used to
						pad a log block full */
#define MLOG_FILE_CREATE	((byte)33)	/* log record about an .ibd
						file creation */
#define MLOG_FILE_RENAME	((byte)34)	/* log record about an .ibd
						file rename */
#define MLOG_FILE_DELETE	((byte)35)	/* log record about an .ibd
						file deletion */
#define MLOG_COMP_REC_MIN_MARK	((byte)36)	/* mark a compact index record
						as the predefined minimum
						record */
#define MLOG_COMP_PAGE_CREATE	((byte)37)	/* create a compact
						index page */
#define MLOG_COMP_REC_INSERT	((byte)38)	/* compact record insert */
#define MLOG_COMP_REC_CLUST_DELETE_MARK ((byte)39)
						/* mark compact clustered index
						record deleted */
#define MLOG_COMP_REC_SEC_DELETE_MARK ((byte)40)/* mark compact secondary index
						record deleted; this log
						record type is redundant, as
						MLOG_REC_SEC_DELETE_MARK is
						independent of the record
						format. */
#define MLOG_COMP_REC_UPDATE_IN_PLACE ((byte)41)/* update of a compact record,
						preserves record field sizes */
#define MLOG_COMP_REC_DELETE	((byte)42)	/* delete a compact record
						from a page */
#define MLOG_COMP_LIST_END_DELETE ((byte)43)	/* delete compact record list
						end on index page */
#define MLOG_COMP_LIST_START_DELETE ((byte)44)	/* delete compact record list
						start on index page */
#define MLOG_COMP_LIST_END_COPY_CREATED ((byte)45)
						/* copy compact record list end
						to a new created index page */
#define MLOG_COMP_PAGE_REORGANIZE ((byte)46)	/* reorganize an index page */
#define MLOG_COMP_DECOMPRESS ((byte)47)		/* decompress a page
						to undo a compressed page
						overflow */

#define MLOG_BIGGEST_TYPE	((byte)47)	/* biggest value (used in
						asserts) */
					
/*******************************************************************
Starts a mini-transaction and creates a mini-transaction handle 
and buffer in the memory buffer given by the caller. */
UNIV_INLINE
mtr_t*
mtr_start(
/*======*/
			/* out: mtr buffer which also acts as
			the mtr handle */
	mtr_t*	mtr);	/* in: memory buffer for the mtr buffer */
/*******************************************************************
Starts a mini-transaction and creates a mini-transaction handle 
and buffer in the memory buffer given by the caller. */

mtr_t*
mtr_start_noninline(
/*================*/
			/* out: mtr buffer which also acts as
			the mtr handle */
	mtr_t*	mtr);	/* in: memory buffer for the mtr buffer */
/*******************************************************************
Commits a mini-transaction. */

void
mtr_commit(
/*=======*/
	mtr_t*	mtr);	/* in: mini-transaction */
/**************************************************************
Sets and returns a savepoint in mtr. */
UNIV_INLINE
ulint
mtr_set_savepoint(
/*==============*/
			/* out: savepoint */
	mtr_t*	mtr);	/* in: mtr */
/**************************************************************
Releases the latches stored in an mtr memo down to a savepoint.
NOTE! The mtr must not have made changes to buffer pages after the
savepoint, as these can be handled only by mtr_commit. */

void
mtr_rollback_to_savepoint(
/*======================*/
	mtr_t*	mtr,		/* in: mtr */
	ulint	savepoint);	/* in: savepoint */
/**************************************************************
Releases the (index tree) s-latch stored in an mtr memo after a
savepoint. */
UNIV_INLINE
void
mtr_release_s_latch_at_savepoint(
/*=============================*/
	mtr_t*		mtr,		/* in: mtr */
	ulint		savepoint,	/* in: savepoint */
	rw_lock_t* 	lock);		/* in: latch to release */
/*******************************************************************
Gets the logging mode of a mini-transaction. */
UNIV_INLINE
ulint
mtr_get_log_mode(
/*=============*/
			/* out: logging mode: MTR_LOG_NONE, ... */
	mtr_t*	mtr);	/* in: mtr */
/*******************************************************************
Changes the logging mode of a mini-transaction. */
UNIV_INLINE
ulint
mtr_set_log_mode(
/*=============*/
			/* out: old mode */
	mtr_t*	mtr,	/* in: mtr */
	ulint	mode);	/* in: logging mode: MTR_LOG_NONE, ... */
/************************************************************
Reads 1 - 4 bytes from a file page buffered in the buffer pool. */

ulint
mtr_read_ulint(
/*===========*/
			/* out: value read */
	byte*	ptr,	/* in: pointer from where to read */
	ulint	type,	/* in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Reads 8 bytes from a file page buffered in the buffer pool. */

dulint
mtr_read_dulint(
/*===========*/
			/* out: value read */
	byte*	ptr,	/* in: pointer from where to read */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/*************************************************************************
This macro locks an rw-lock in s-mode. */
#define mtr_s_lock(B, MTR)	mtr_s_lock_func((B), __FILE__, __LINE__,\
						(MTR))
/*************************************************************************
This macro locks an rw-lock in x-mode. */
#define mtr_x_lock(B, MTR)	mtr_x_lock_func((B), __FILE__, __LINE__,\
						(MTR))
/*************************************************************************
NOTE! Use the macro above!
Locks a lock in s-mode. */
UNIV_INLINE
void
mtr_s_lock_func(
/*============*/
	rw_lock_t*	lock,	/* in: rw-lock */
	const char*	file,	/* in: file name */
	ulint		line,	/* in: line number */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************************
NOTE! Use the macro above!
Locks a lock in x-mode. */
UNIV_INLINE
void
mtr_x_lock_func(
/*============*/
	rw_lock_t*	lock,	/* in: rw-lock */
	const char*	file,	/* in: file name */
	ulint		line,	/* in: line number */
	mtr_t*		mtr);	/* in: mtr */

/*******************************************************
Releases an object in the memo stack. */

void
mtr_memo_release(
/*=============*/
	mtr_t*	mtr,	/* in: mtr */
	void*	object,	/* in: object */
	ulint	type);	/* in: object type: MTR_MEMO_S_LOCK, ... */
/**************************************************************
Checks if memo contains the given item. */
UNIV_INLINE
ibool
mtr_memo_contains(
/*==============*/
			/* out: TRUE if contains */
	mtr_t*	mtr,	/* in: mtr */
	void*	object,	/* in: object to search */
	ulint	type);	/* in: type of object */
/*************************************************************
Prints info of an mtr handle. */

void
mtr_print(
/*======*/
	mtr_t*	mtr);	/* in: mtr */
/*######################################################################*/

#define	MTR_BUF_MEMO_SIZE	200	/* number of slots in memo */

/*******************************************************************
Returns the log object of a mini-transaction buffer. */
UNIV_INLINE
dyn_array_t*
mtr_get_log(
/*========*/
			/* out: log */
	mtr_t*	mtr);	/* in: mini-transaction */
/*******************************************************
Pushes an object to an mtr memo stack. */
UNIV_INLINE
void
mtr_memo_push(
/*==========*/
	mtr_t*	mtr,	/* in: mtr */
	void*	object,	/* in: object */
	ulint	type);	/* in: object type: MTR_MEMO_S_LOCK, ... */


/* Type definition of a mini-transaction memo stack slot. */
typedef	struct mtr_memo_slot_struct	mtr_memo_slot_t;
struct mtr_memo_slot_struct{
	ulint	type;	/* type of the stored object (MTR_MEMO_S_LOCK, ...) */
	void*	object;	/* pointer to the object */
};

/* Mini-transaction handle and buffer */
struct mtr_struct{
	ulint		state;	/* MTR_ACTIVE, MTR_COMMITTING, MTR_COMMITTED */
	dyn_array_t	memo;	/* memo stack for locks etc. */
	dyn_array_t	log;	/* mini-transaction log */
	ibool		modifications;
				/* TRUE if the mtr made modifications to
				buffer pool pages */
	ulint		n_log_recs;
				/* count of how many page initial log records
				have been written to the mtr log */
	ulint		log_mode; /* specifies which operations should be
				logged; default value MTR_LOG_ALL */
	dulint		start_lsn;/* start lsn of the possible log entry for
				this mtr */
	dulint		end_lsn;/* end lsn of the possible log entry for
				this mtr */
	ulint		magic_n;
};

#define	MTR_MAGIC_N		54551

#define MTR_ACTIVE		12231
#define MTR_COMMITTING		56456
#define MTR_COMMITTED		34676
	
#ifndef UNIV_NONINL
#include "mtr0mtr.ic"
#endif

#endif
