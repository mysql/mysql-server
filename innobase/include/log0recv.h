/******************************************************
Recovery

(c) 1997 Innobase Oy

Created 9/20/1997 Heikki Tuuri
*******************************************************/

#ifndef log0recv_h
#define log0recv_h

#include "univ.i"
#include "ut0byte.h"
#include "page0types.h"
#include "hash0hash.h"
#include "log0log.h"

/***********************************************************************
Returns TRUE if recovery is currently running. */
UNIV_INLINE
ibool
recv_recovery_is_on(void);
/*=====================*/
/***********************************************************************
Returns TRUE if recovery from backup is currently running. */
UNIV_INLINE
ibool
recv_recovery_from_backup_is_on(void);
/*=================================*/
/****************************************************************************
Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool. */

void
recv_recover_page(
/*==============*/
	ibool	just_read_in,	/* in: TRUE if the i/o-handler calls this for
				a freshly read page */
	page_t*	page,		/* in: buffer page */
	ulint	space,		/* in: space id */
	ulint	page_no);	/* in: page number */
/************************************************************
Recovers from a checkpoint. When this function returns, the database is able
to start processing of new user transactions, but the function
recv_recovery_from_checkpoint_finish should be called later to complete
the recovery and free the resources used in it. */

ulint
recv_recovery_from_checkpoint_start(
/*================================*/
				/* out: error code or DB_SUCCESS */
	ulint	type,		/* in: LOG_CHECKPOINT or LOG_ARCHIVE */
	dulint	limit_lsn,	/* in: recover up to this lsn if possible */
	dulint	min_flushed_lsn,/* in: min flushed lsn from data files */
	dulint	max_flushed_lsn);/* in: max flushed lsn from data files */
/************************************************************
Completes recovery from a checkpoint. */

void
recv_recovery_from_checkpoint_finish(void);
/*======================================*/
/***********************************************************
Scans log from a buffer and stores new log data to the parsing buffer. Parses
and hashes the log records if new data found. */

ibool
recv_scan_log_recs(
/*===============*/
				/* out: TRUE if limit_lsn has been reached, or
				not able to scan any more in this log group */
	ibool	store_to_hash,	/* in: TRUE if the records should be stored
				to the hash table; this is set FALSE if just
				debug checking is needed */
	byte*	buf,		/* in: buffer containing a log segment or
				garbage */
	ulint	len,		/* in: buffer length */
	dulint	start_lsn,	/* in: buffer start lsn */
	dulint*	contiguous_lsn,	/* in/out: it is known that all log groups
				contain contiguous log data up to this lsn */
	dulint*	group_scanned_lsn);/* out: scanning succeeded up to this lsn */
/**********************************************************
Resets the logs. The contents of log files will be lost! */

void
recv_reset_logs(
/*============*/
	dulint	lsn,		/* in: reset to this lsn rounded up to
				be divisible by OS_FILE_LOG_BLOCK_SIZE,
				after which we add LOG_BLOCK_HDR_SIZE */
	ulint	arch_log_no,	/* in: next archived log file number */
	ibool	new_logs_created);/* in: TRUE if resetting logs is done
				at the log creation; FALSE if it is done
				after archive recovery */
/************************************************************
Creates the recovery system. */

void
recv_sys_create(void);
/*=================*/
/************************************************************
Inits the recovery system for a recovery operation. */

void
recv_sys_init(void);
/*===============*/
/***********************************************************************
Empties the hash table of stored log records, applying them to appropriate
pages. */

void
recv_apply_hashed_log_recs(
/*=======================*/
	ibool	allow_ibuf);	/* in: if TRUE, also ibuf operations are
				allowed during the application; if FALSE,
				no ibuf operations are allowed, and after
				the application all file pages are flushed to
				disk and invalidated in buffer pool: this
				alternative means that no new log records
				can be generated during the application */
/************************************************************
Recovers from archived log files, and also from log files, if they exist. */

ulint
recv_recovery_from_archive_start(
/*=============================*/
				/* out: error code or DB_SUCCESS */
	dulint	min_flushed_lsn,/* in: min flushed lsn field from the
				data files */
	dulint	limit_lsn,	/* in: recover up to this lsn if possible */
	ulint	first_log_no);	/* in: number of the first archived log file
				to use in the recovery; the file will be
				searched from INNOBASE_LOG_ARCH_DIR specified
				in server config file */
/************************************************************
Completes recovery from archive. */

void
recv_recovery_from_archive_finish(void);
/*===================================*/
/***********************************************************************
Checks that a replica of a space is identical to the original space. */

void
recv_compare_spaces(
/*================*/
	ulint	space1,	/* in: space id */
	ulint	space2,	/* in: space id */
	ulint	n_pages);/* in: number of pages */
/***********************************************************************
Checks that a replica of a space is identical to the original space. Disables
ibuf operations and flushes and invalidates the buffer pool pages after the
test. This function can be used to check the recovery before dict or trx
systems are initialized. */

void
recv_compare_spaces_low(
/*====================*/
	ulint	space1,	/* in: space id */
	ulint	space2,	/* in: space id */
	ulint	n_pages);/* in: number of pages */

/* Block of log record data */
typedef struct recv_data_struct	recv_data_t;
struct recv_data_struct{
	recv_data_t*	next;	/* pointer to the next block or NULL */
				/* the log record data is stored physically
				immediately after this struct, max amount
				RECV_DATA_BLOCK_SIZE bytes of it */
};

/* Stored log record struct */
typedef struct recv_struct	recv_t;
struct recv_struct{
	byte		type;	/* log record type */
	ulint		len;	/* log record body length in bytes */
	recv_data_t*	data;	/* chain of blocks containing the log record
				body */
	dulint		start_lsn;/* start lsn of the log segment written by
				the mtr which generated this log record: NOTE
				that this is not necessarily the start lsn of
				this log record */
	dulint		end_lsn;/* end lsn of the log segment written by
				the mtr which generated this log record: NOTE
				that this is not necessarily the end lsn of
				this log record */
	UT_LIST_NODE_T(recv_t)
			rec_list;/* list of log records for this page */
};

/* Hashed page file address struct */
typedef struct recv_addr_struct	recv_addr_t;
struct recv_addr_struct{
	ulint		state;	/* RECV_NOT_PROCESSED, RECV_BEING_PROCESSED,
				or RECV_PROCESSED */
	ulint		space;	/* space id */
	ulint		page_no;/* page number */
	UT_LIST_BASE_NODE_T(recv_t)
			rec_list;/* list of log records for this page */
	hash_node_t	addr_hash;
};

/* Recovery system data structure */
typedef struct recv_sys_struct	recv_sys_t;
struct recv_sys_struct{
	mutex_t		mutex;	/* mutex protecting the fields apply_log_recs,
				n_addrs, and the state field in each recv_addr
				struct */
	ibool		apply_log_recs;
				/* this is TRUE when log rec application to
				pages is allowed; this flag tells the
				i/o-handler if it should do log record
				application */
	ibool		apply_batch_on;
				/* this is TRUE when a log rec application
				batch is running */
	dulint		lsn;	/* log sequence number */
	ulint		last_log_buf_size;
				/* size of the log buffer when the database
				last time wrote to the log */
	byte*		last_block;
				/* possible incomplete last recovered log
				block */
	byte*		last_block_buf_start;
				/* the nonaligned start address of the
				preceding buffer */
	byte*		buf;	/* buffer for parsing log records */
	ulint		len;	/* amount of data in buf */
	dulint		parse_start_lsn;
				/* this is the lsn from which we were able to
				start parsing log records and adding them to
				the hash table; ut_dulint_zero if a suitable
				start point not found yet */
	dulint		scanned_lsn;
				/* the log data has been scanned up to this
				lsn */
	ulint		scanned_checkpoint_no;
				/* the log data has been scanned up to this
				checkpoint number (lowest 4 bytes) */
	ulint		recovered_offset;
				/* start offset of non-parsed log records in
				buf */
	dulint		recovered_lsn;
				/* the log records have been parsed up to
				this lsn */
	dulint		limit_lsn;/* recovery should be made at most up to this
				lsn */
	log_group_t*	archive_group;
				/* in archive recovery: the log group whose
				archive is read */
	mem_heap_t*	heap;	/* memory heap of log records and file
				addresses*/
	hash_table_t*	addr_hash;/* hash table of file addresses of pages */
	ulint		n_addrs;/* number of not processed hashed file
				addresses in the hash table */
};

extern recv_sys_t*	recv_sys;
extern ibool		recv_recovery_on;
extern ibool		recv_no_ibuf_operations;

/* States of recv_addr_struct */
#define RECV_NOT_PROCESSED	71
#define RECV_BEING_READ		72
#define RECV_BEING_PROCESSED	73
#define RECV_PROCESSED		74

/* The number which is added to a space id to obtain the replicate space
in the debug version: spaces with an odd number as the id are replicate
spaces */
#define RECV_REPLICA_SPACE_ADD	1

/* This many blocks must be left free in the buffer pool when we scan
the log and store the scanned log records in the buffer pool: we will
use these free blocks to read in pages when we start applying the
log records to the database. */

#define RECV_POOL_N_FREE_BLOCKS	 (ut_min(256, buf_pool_get_curr_size() / 8))

#ifndef UNIV_NONINL
#include "log0recv.ic"
#endif

#endif
