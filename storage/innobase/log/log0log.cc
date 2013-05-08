/*****************************************************************************

Copyright (c) 1995, 2013, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2009, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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
@file log/log0log.cc
Database log

Created 12/9/1995 Heikki Tuuri
*******************************************************/

#include "log0log.h"

#ifdef UNIV_NONINL
#include "log0log.ic"
#endif

#ifndef UNIV_HOTBACKUP
#include "mem0mem.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "srv0srv.h"
#include "log0recv.h"
#include "fil0fil.h"
#include "dict0boot.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0sys.h"
#include "trx0trx.h"
#include "srv0mon.h"

/*
General philosophy of InnoDB redo-logs:

1) Every change to a contents of a data page must be done
through mtr, which in mtr_commit() writes log records
to the InnoDB redo log.

2) Normally these changes are performed using a mlog_write_ulint()
or similar function.

3) In some page level operations only a code number of a
c-function and its parameters are written to the log to
reduce the size of the log.

  3a) You should not add parameters to these kind of functions
  (e.g. trx_undo_header_create(), trx_undo_insert_header_reuse())

  3b) You should not add such functionality which either change
  working when compared with the old or are dependent on data
  outside of the page. These kind of functions should implement
  self-contained page transformation and it should be unchanged
  if you don't have very essential reasons to change log
  semantics or format.

*/

/* Global log system variable */
UNIV_INTERN log_t*	log_sys	= NULL;

#ifdef UNIV_PFS_RWLOCK
UNIV_INTERN mysql_pfs_key_t	checkpoint_lock_key;
#endif /* UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	log_sys_mutex_key;
UNIV_INTERN mysql_pfs_key_t	log_flush_order_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_DEBUG
UNIV_INTERN ibool	log_do_write = TRUE;
#endif /* UNIV_DEBUG */

/* These control how often we print warnings if the last checkpoint is too
old */
UNIV_INTERN ibool	log_has_printed_chkp_warning = FALSE;
UNIV_INTERN time_t	log_last_warning_time;

/* A margin for free space in the log buffer before a log entry is catenated */
#define LOG_BUF_WRITE_MARGIN	(4 * OS_FILE_LOG_BLOCK_SIZE)

/* Margins for free space in the log buffer after a log entry is catenated */
#define LOG_BUF_FLUSH_RATIO	2
#define LOG_BUF_FLUSH_MARGIN	(LOG_BUF_WRITE_MARGIN + 4 * UNIV_PAGE_SIZE)

/* Margin for the free space in the smallest log group, before a new query
step which modifies the database, is started */

#define LOG_CHECKPOINT_FREE_PER_THREAD	(4 * UNIV_PAGE_SIZE)
#define LOG_CHECKPOINT_EXTRA_FREE	(8 * UNIV_PAGE_SIZE)

/* This parameter controls asynchronous making of a new checkpoint; the value
should be bigger than LOG_POOL_PREFLUSH_RATIO_SYNC */

#define LOG_POOL_CHECKPOINT_RATIO_ASYNC	32

/* This parameter controls synchronous preflushing of modified buffer pages */
#define LOG_POOL_PREFLUSH_RATIO_SYNC	16

/* The same ratio for asynchronous preflushing; this value should be less than
the previous */
#define LOG_POOL_PREFLUSH_RATIO_ASYNC	8

/* Extra margin, in addition to one log file, used in archiving */
#define LOG_ARCHIVE_EXTRA_MARGIN	(4 * UNIV_PAGE_SIZE)

/* This parameter controls asynchronous writing to the archive */
#define LOG_ARCHIVE_RATIO_ASYNC		16

/* Codes used in unlocking flush latches */
#define LOG_UNLOCK_NONE_FLUSHED_LOCK	1
#define LOG_UNLOCK_FLUSH_LOCK		2

/* States of an archiving operation */
#define	LOG_ARCHIVE_READ	1
#define	LOG_ARCHIVE_WRITE	2

/******************************************************//**
Completes a checkpoint write i/o to a log file. */
static
void
log_io_complete_checkpoint(void);
/*============================*/

/****************************************************************//**
Returns the oldest modified block lsn in the pool, or log_sys->lsn if none
exists.
@return	LSN of oldest modification */
static
lsn_t
log_buf_pool_get_oldest_modification(void)
/*======================================*/
{
	lsn_t	lsn;

	ut_ad(mutex_own(&(log_sys->mutex)));

	lsn = buf_pool_get_oldest_modification();

	if (!lsn) {

		lsn = log_sys->lsn;
	}

	return(lsn);
}

/************************************************************//**
Opens the log for log_write_low. The log must be closed with log_close and
released with log_release.
@return	start lsn of the log record */
UNIV_INTERN
lsn_t
log_reserve_and_open(
/*=================*/
	ulint	len)	/*!< in: length of data to be catenated */
{
	log_t*	log			= log_sys;
	ulint	len_upper_limit;
#ifdef UNIV_DEBUG
	ulint	count			= 0;
#endif /* UNIV_DEBUG */

	ut_a(len < log->buf_size / 2);
loop:
	mutex_enter(&(log->mutex));
	ut_ad(!recv_no_log_write);

	/* Calculate an upper limit for the space the string may take in the
	log buffer */

	len_upper_limit = LOG_BUF_WRITE_MARGIN + (5 * len) / 4;

	if (log->buf_free + len_upper_limit > log->buf_size) {

		mutex_exit(&(log->mutex));

		/* Not enough free space, do a syncronous flush of the log
		buffer */

		log_buffer_flush_to_disk();

		srv_stats.log_waits.inc();

		ut_ad(++count < 50);

		goto loop;
	}

#ifdef UNIV_LOG_DEBUG
	log->old_buf_free = log->buf_free;
	log->old_lsn = log->lsn;
#endif
	return(log->lsn);
}

/************************************************************//**
Writes to the log the string given. It is assumed that the caller holds the
log mutex. */
UNIV_INTERN
void
log_write_low(
/*==========*/
	byte*	str,		/*!< in: string */
	ulint	str_len)	/*!< in: string length */
{
	log_t*	log	= log_sys;
	ulint	len;
	ulint	data_len;
	byte*	log_block;

	ut_ad(mutex_own(&(log->mutex)));
part_loop:
	ut_ad(!recv_no_log_write);
	/* Calculate a part length */

	data_len = (log->buf_free % OS_FILE_LOG_BLOCK_SIZE) + str_len;

	if (data_len <= OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {

		/* The string fits within the current log block */

		len = str_len;
	} else {
		data_len = OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE;

		len = OS_FILE_LOG_BLOCK_SIZE
			- (log->buf_free % OS_FILE_LOG_BLOCK_SIZE)
			- LOG_BLOCK_TRL_SIZE;
	}

	ut_memcpy(log->buf + log->buf_free, str, len);

	str_len -= len;
	str = str + len;

	log_block = static_cast<byte*>(
		ut_align_down(
			log->buf + log->buf_free, OS_FILE_LOG_BLOCK_SIZE));

	log_block_set_data_len(log_block, data_len);

	if (data_len == OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {
		/* This block became full */
		log_block_set_data_len(log_block, OS_FILE_LOG_BLOCK_SIZE);
		log_block_set_checkpoint_no(log_block,
					    log_sys->next_checkpoint_no);
		len += LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE;

		log->lsn += len;

		/* Initialize the next block header */
		log_block_init(log_block + OS_FILE_LOG_BLOCK_SIZE, log->lsn);
	} else {
		log->lsn += len;
	}

	log->buf_free += len;

	ut_ad(log->buf_free <= log->buf_size);

	if (str_len > 0) {
		goto part_loop;
	}

	srv_stats.log_write_requests.inc();
}

/************************************************************//**
Closes the log.
@return	lsn */
UNIV_INTERN
lsn_t
log_close(void)
/*===========*/
{
	byte*		log_block;
	ulint		first_rec_group;
	lsn_t		oldest_lsn;
	lsn_t		lsn;
	log_t*		log	= log_sys;
	lsn_t		checkpoint_age;

	ut_ad(mutex_own(&(log->mutex)));
	ut_ad(!recv_no_log_write);

	lsn = log->lsn;

	log_block = static_cast<byte*>(
		ut_align_down(
			log->buf + log->buf_free, OS_FILE_LOG_BLOCK_SIZE));

	first_rec_group = log_block_get_first_rec_group(log_block);

	if (first_rec_group == 0) {
		/* We initialized a new log block which was not written
		full by the current mtr: the next mtr log record group
		will start within this block at the offset data_len */

		log_block_set_first_rec_group(
			log_block, log_block_get_data_len(log_block));
	}

	if (log->buf_free > log->max_buf_free) {

		log->check_flush_or_checkpoint = TRUE;
	}

	checkpoint_age = lsn - log->last_checkpoint_lsn;

	if (checkpoint_age >= log->log_group_capacity) {
		/* TODO: split btr_store_big_rec_extern_fields() into small
		steps so that we can release all latches in the middle, and
		call log_free_check() to ensure we never write over log written
		after the latest checkpoint. In principle, we should split all
		big_rec operations, but other operations are smaller. */

		if (!log_has_printed_chkp_warning
		    || difftime(time(NULL), log_last_warning_time) > 15) {

			log_has_printed_chkp_warning = TRUE;
			log_last_warning_time = time(NULL);

			ut_print_timestamp(stderr);
			ib_logf(IB_LOG_LEVEL_ERROR,
				"The age of the last checkpoint is "
				LSN_PF ", which exceeds the log group"
				" capacity " LSN_PF ".  If you are using"
				" big BLOB or TEXT rows, you must set the"
				" combined size of log files at least 10"
				" times bigger than the largest such row.",
				checkpoint_age,
				log->log_group_capacity);
		}
	}

	if (checkpoint_age <= log->max_modified_age_sync) {

		goto function_exit;
	}

	oldest_lsn = buf_pool_get_oldest_modification();

	if (!oldest_lsn
	    || lsn - oldest_lsn > log->max_modified_age_sync
	    || checkpoint_age > log->max_checkpoint_age_async) {

		log->check_flush_or_checkpoint = TRUE;
	}
function_exit:

#ifdef UNIV_LOG_DEBUG
	log_check_log_recs(log->buf + log->old_buf_free,
			   log->buf_free - log->old_buf_free, log->old_lsn);
#endif

	return(lsn);
}

/******************************************************//**
Calculates the data capacity of a log group, when the log file headers are not
included.
@return	capacity in bytes */
UNIV_INTERN
lsn_t
log_group_get_capacity(
/*===================*/
	const log_group_t*	group)	/*!< in: log group */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	return((group->file_size - LOG_FILE_HDR_SIZE) * group->n_files);
}

/******************************************************//**
Calculates the offset within a log group, when the log file headers are not
included.
@return	size offset (<= offset) */
UNIV_INLINE
lsn_t
log_group_calc_size_offset(
/*=======================*/
	lsn_t			offset,	/*!< in: real offset within the
					log group */
	const log_group_t*	group)	/*!< in: log group */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	return(offset - LOG_FILE_HDR_SIZE * (1 + offset / group->file_size));
}

/******************************************************//**
Calculates the offset within a log group, when the log file headers are
included.
@return	real offset (>= offset) */
UNIV_INLINE
lsn_t
log_group_calc_real_offset(
/*=======================*/
	lsn_t			offset,	/*!< in: size offset within the
					log group */
	const log_group_t*	group)	/*!< in: log group */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	return(offset + LOG_FILE_HDR_SIZE
	       * (1 + offset / (group->file_size - LOG_FILE_HDR_SIZE)));
}

/******************************************************//**
Calculates the offset of an lsn within a log group.
@return	offset within the log group */
static
lsn_t
log_group_calc_lsn_offset(
/*======================*/
	lsn_t			lsn,	/*!< in: lsn */
	const log_group_t*	group)	/*!< in: log group */
{
	lsn_t	gr_lsn;
	lsn_t	gr_lsn_size_offset;
	lsn_t	difference;
	lsn_t	group_size;
	lsn_t	offset;

	ut_ad(mutex_own(&(log_sys->mutex)));

	gr_lsn = group->lsn;

	gr_lsn_size_offset = log_group_calc_size_offset(group->lsn_offset, group);

	group_size = log_group_get_capacity(group);

	if (lsn >= gr_lsn) {

		difference = lsn - gr_lsn;
	} else {
		difference = gr_lsn - lsn;

		difference = difference % group_size;

		difference = group_size - difference;
	}

	offset = (gr_lsn_size_offset + difference) % group_size;

	/* fprintf(stderr,
	"Offset is " LSN_PF " gr_lsn_offset is " LSN_PF
	" difference is " LSN_PF "\n",
	offset, gr_lsn_size_offset, difference);
	*/

	return(log_group_calc_real_offset(offset, group));
}
#endif /* !UNIV_HOTBACKUP */

/*******************************************************************//**
Calculates where in log files we find a specified lsn.
@return	log file number */
UNIV_INTERN
ulint
log_calc_where_lsn_is(
/*==================*/
	ib_int64_t*	log_file_offset,	/*!< out: offset in that file
						(including the header) */
	ib_uint64_t	first_header_lsn,	/*!< in: first log file start
						lsn */
	ib_uint64_t	lsn,			/*!< in: lsn whose position to
						determine */
	ulint		n_log_files,		/*!< in: total number of log
						files */
	ib_int64_t	log_file_size)		/*!< in: log file size
						(including the header) */
{
	ib_int64_t	capacity	= log_file_size - LOG_FILE_HDR_SIZE;
	ulint		file_no;
	ib_int64_t	add_this_many;

	if (lsn < first_header_lsn) {
		add_this_many = 1 + (first_header_lsn - lsn)
			/ (capacity * (ib_int64_t) n_log_files);
		lsn += add_this_many
			* capacity * (ib_int64_t) n_log_files;
	}

	ut_a(lsn >= first_header_lsn);

	file_no = ((ulint)((lsn - first_header_lsn) / capacity))
		% n_log_files;
	*log_file_offset = (lsn - first_header_lsn) % capacity;

	*log_file_offset = *log_file_offset + LOG_FILE_HDR_SIZE;

	return(file_no);
}

#ifndef UNIV_HOTBACKUP
/********************************************************//**
Sets the field values in group to correspond to a given lsn. For this function
to work, the values must already be correctly initialized to correspond to
some lsn, for instance, a checkpoint lsn. */
UNIV_INTERN
void
log_group_set_fields(
/*=================*/
	log_group_t*	group,	/*!< in/out: group */
	lsn_t		lsn)	/*!< in: lsn for which the values should be
				set */
{
	group->lsn_offset = log_group_calc_lsn_offset(lsn, group);
	group->lsn = lsn;
}

/*****************************************************************//**
Calculates the recommended highest values for lsn - last_checkpoint_lsn,
lsn - buf_get_oldest_modification(), and lsn - max_archive_lsn_age.
@retval true on success
@retval false if the smallest log group is too small to
accommodate the number of OS threads in the database server */
static __attribute__((warn_unused_result))
bool
log_calc_max_ages(void)
/*===================*/
{
	log_group_t*	group;
	lsn_t		margin;
	ulint		free;
	bool		success	= true;
	lsn_t		smallest_capacity;
	lsn_t		archive_margin;
	lsn_t		smallest_archive_margin;

	mutex_enter(&(log_sys->mutex));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	ut_ad(group);

	smallest_capacity = LSN_MAX;
	smallest_archive_margin = LSN_MAX;

	while (group) {
		if (log_group_get_capacity(group) < smallest_capacity) {

			smallest_capacity = log_group_get_capacity(group);
		}

		archive_margin = log_group_get_capacity(group)
			- (group->file_size - LOG_FILE_HDR_SIZE)
			- LOG_ARCHIVE_EXTRA_MARGIN;

		if (archive_margin < smallest_archive_margin) {

			smallest_archive_margin = archive_margin;
		}

		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	/* Add extra safety */
	smallest_capacity = smallest_capacity - smallest_capacity / 10;

	/* For each OS thread we must reserve so much free space in the
	smallest log group that it can accommodate the log entries produced
	by single query steps: running out of free log space is a serious
	system error which requires rebooting the database. */

	free = LOG_CHECKPOINT_FREE_PER_THREAD * (10 + srv_thread_concurrency)
		+ LOG_CHECKPOINT_EXTRA_FREE;
	if (free >= smallest_capacity / 2) {
		success = false;

		goto failure;
	} else {
		margin = smallest_capacity - free;
	}

	margin = margin - margin / 10;	/* Add still some extra safety */

	log_sys->log_group_capacity = smallest_capacity;

	log_sys->max_modified_age_async = margin
		- margin / LOG_POOL_PREFLUSH_RATIO_ASYNC;
	log_sys->max_modified_age_sync = margin
		- margin / LOG_POOL_PREFLUSH_RATIO_SYNC;

	log_sys->max_checkpoint_age_async = margin - margin
		/ LOG_POOL_CHECKPOINT_RATIO_ASYNC;
	log_sys->max_checkpoint_age = margin;

failure:
	mutex_exit(&(log_sys->mutex));

	if (!success) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot continue operation.  ib_logfiles are too"
			" small for innodb_thread_concurrency %lu. The"
			" combined size of ib_logfiles should be bigger than"
			" 200 kB * innodb_thread_concurrency. To get mysqld"
			" to start up, set innodb_thread_concurrency in"
			" my.cnf to a lower value, for example, to 8. After"
			" an ERROR-FREE shutdown of mysqld you can adjust"
			" the size of ib_logfiles, as explained in " REFMAN
			"adding-and-removing.html.",
			(ulong) srv_thread_concurrency);
	}

	return(success);
}

/******************************************************//**
Initializes the log. */
UNIV_INTERN
void
log_init(void)
/*==========*/
{
	log_sys = static_cast<log_t*>(mem_alloc(sizeof(log_t)));

	mutex_create(log_sys_mutex_key, &log_sys->mutex, SYNC_LOG);

	mutex_create(log_flush_order_mutex_key,
		     &log_sys->log_flush_order_mutex,
		     SYNC_LOG_FLUSH_ORDER);

	mutex_enter(&(log_sys->mutex));

	/* Start the lsn from one log block from zero: this way every
	log record has a start lsn != zero, a fact which we will use */

	log_sys->lsn = LOG_START_LSN;

	ut_a(LOG_BUFFER_SIZE >= 16 * OS_FILE_LOG_BLOCK_SIZE);
	ut_a(LOG_BUFFER_SIZE >= 4 * UNIV_PAGE_SIZE);

	log_sys->buf_ptr = static_cast<byte*>(
		mem_zalloc(LOG_BUFFER_SIZE + OS_FILE_LOG_BLOCK_SIZE));

	log_sys->buf = static_cast<byte*>(
		ut_align(log_sys->buf_ptr, OS_FILE_LOG_BLOCK_SIZE));

	log_sys->buf_size = LOG_BUFFER_SIZE;

	log_sys->max_buf_free = log_sys->buf_size / LOG_BUF_FLUSH_RATIO
		- LOG_BUF_FLUSH_MARGIN;
	log_sys->check_flush_or_checkpoint = TRUE;
	UT_LIST_INIT(log_sys->log_groups);

	log_sys->n_log_ios = 0;

	log_sys->n_log_ios_old = log_sys->n_log_ios;
	log_sys->last_printout_time = time(NULL);
	/*----------------------------*/

	log_sys->buf_next_to_write = 0;

	log_sys->write_lsn = 0;
	log_sys->current_flush_lsn = 0;
	log_sys->flushed_to_disk_lsn = 0;

	log_sys->written_to_some_lsn = log_sys->lsn;
	log_sys->written_to_all_lsn = log_sys->lsn;

	log_sys->n_pending_writes = 0;

	log_sys->no_flush_event = os_event_create();

	os_event_set(log_sys->no_flush_event);

	log_sys->one_flushed_event = os_event_create();

	os_event_set(log_sys->one_flushed_event);

	/*----------------------------*/

	log_sys->next_checkpoint_no = 0;
	log_sys->last_checkpoint_lsn = log_sys->lsn;
	log_sys->n_pending_checkpoint_writes = 0;


	rw_lock_create(checkpoint_lock_key, &log_sys->checkpoint_lock,
		       SYNC_NO_ORDER_CHECK);

	log_sys->checkpoint_buf_ptr = static_cast<byte*>(
		mem_zalloc(2 * OS_FILE_LOG_BLOCK_SIZE));

	log_sys->checkpoint_buf = static_cast<byte*>(
		ut_align(log_sys->checkpoint_buf_ptr, OS_FILE_LOG_BLOCK_SIZE));

	/*----------------------------*/

	log_block_init(log_sys->buf, log_sys->lsn);
	log_block_set_first_rec_group(log_sys->buf, LOG_BLOCK_HDR_SIZE);

	log_sys->buf_free = LOG_BLOCK_HDR_SIZE;
	log_sys->lsn = LOG_START_LSN + LOG_BLOCK_HDR_SIZE;

	MONITOR_SET(MONITOR_LSN_CHECKPOINT_AGE,
		    log_sys->lsn - log_sys->last_checkpoint_lsn);

	mutex_exit(&(log_sys->mutex));

#ifdef UNIV_LOG_DEBUG
	recv_sys_create();
	recv_sys_init(buf_pool_get_curr_size());

	recv_sys->parse_start_lsn = log_sys->lsn;
	recv_sys->scanned_lsn = log_sys->lsn;
	recv_sys->scanned_checkpoint_no = 0;
	recv_sys->recovered_lsn = log_sys->lsn;
	recv_sys->limit_lsn = LSN_MAX;
#endif
}

/******************************************************************//**
Inits a log group to the log system.
@return true if success, false if not */
UNIV_INTERN __attribute__((warn_unused_result))
bool
log_group_init(
/*===========*/
	ulint	id,			/*!< in: group id */
	ulint	n_files,		/*!< in: number of log files */
	lsn_t	file_size,		/*!< in: log file size in bytes */
	ulint	space_id,		/*!< in: space id of the file space
					which contains the log files of this
					group */
	ulint	archive_space_id __attribute__((unused)))
					/*!< in: space id of the file space
					which contains some archived log
					files for this group; currently, only
					for the first log group this is
					used */
{
	ulint	i;
	log_group_t*	group;

	group = static_cast<log_group_t*>(mem_alloc(sizeof(log_group_t)));

	group->id = id;
	group->n_files = n_files;
	group->file_size = file_size;
	group->space_id = space_id;
	group->state = LOG_GROUP_OK;
	group->lsn = LOG_START_LSN;
	group->lsn_offset = LOG_FILE_HDR_SIZE;
	group->n_pending_writes = 0;

	group->file_header_bufs_ptr = static_cast<byte**>(
		mem_zalloc(sizeof(byte*) * n_files));

	group->file_header_bufs = static_cast<byte**>(
		mem_zalloc(sizeof(byte**) * n_files));

	for (i = 0; i < n_files; i++) {
		group->file_header_bufs_ptr[i] = static_cast<byte*>(
			mem_zalloc(LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE));

		group->file_header_bufs[i] = static_cast<byte*>(
			ut_align(group->file_header_bufs_ptr[i],
				 OS_FILE_LOG_BLOCK_SIZE));
	}

	group->checkpoint_buf_ptr = static_cast<byte*>(
		mem_zalloc(2 * OS_FILE_LOG_BLOCK_SIZE));

	group->checkpoint_buf = static_cast<byte*>(
		ut_align(group->checkpoint_buf_ptr,OS_FILE_LOG_BLOCK_SIZE));

	UT_LIST_ADD_LAST(log_groups, log_sys->log_groups, group);

	return(log_calc_max_ages());
}

/******************************************************************//**
Does the unlockings needed in flush i/o completion. */
UNIV_INLINE
void
log_flush_do_unlocks(
/*=================*/
	ulint	code)	/*!< in: any ORed combination of LOG_UNLOCK_FLUSH_LOCK
			and LOG_UNLOCK_NONE_FLUSHED_LOCK */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	/* NOTE that we must own the log mutex when doing the setting of the
	events: this is because transactions will wait for these events to
	be set, and at that moment the log flush they were waiting for must
	have ended. If the log mutex were not reserved here, the i/o-thread
	calling this function might be preempted for a while, and when it
	resumed execution, it might be that a new flush had been started, and
	this function would erroneously signal the NEW flush as completed.
	Thus, the changes in the state of these events are performed
	atomically in conjunction with the changes in the state of
	log_sys->n_pending_writes etc. */

	if (code & LOG_UNLOCK_NONE_FLUSHED_LOCK) {
		os_event_set(log_sys->one_flushed_event);
	}

	if (code & LOG_UNLOCK_FLUSH_LOCK) {
		os_event_set(log_sys->no_flush_event);
	}
}

/******************************************************************//**
Checks if a flush is completed for a log group and does the completion
routine if yes.
@return	LOG_UNLOCK_NONE_FLUSHED_LOCK or 0 */
UNIV_INLINE
ulint
log_group_check_flush_completion(
/*=============================*/
	log_group_t*	group)	/*!< in: log group */
{
	ut_ad(mutex_own(&log_sys->mutex));

	if (group->n_pending_writes) {
		return(0);
	}

	if (!log_sys->one_flushed) {
		DBUG_PRINT("ib_log", ("Log flushed first to group %u",
				      unsigned(group->id)));
		log_sys->written_to_some_lsn = log_sys->write_lsn;
		log_sys->one_flushed = TRUE;

		return(LOG_UNLOCK_NONE_FLUSHED_LOCK);
	}

	DBUG_PRINT("ib_log", ("Log flushed to group %u", unsigned(group->id)));
	return(0);
}

/******************************************************//**
Checks if a flush is completed and does the completion routine if yes.
@return	LOG_UNLOCK_FLUSH_LOCK or 0 */
static
ulint
log_sys_check_flush_completion(void)
/*================================*/
{
	ulint	move_start;
	ulint	move_end;

	ut_ad(mutex_own(&(log_sys->mutex)));

	if (log_sys->n_pending_writes == 0) {

		log_sys->written_to_all_lsn = log_sys->write_lsn;
		log_sys->buf_next_to_write = log_sys->write_end_offset;

		if (log_sys->write_end_offset > log_sys->max_buf_free / 2) {
			/* Move the log buffer content to the start of the
			buffer */

			move_start = ut_calc_align_down(
				log_sys->write_end_offset,
				OS_FILE_LOG_BLOCK_SIZE);
			move_end = ut_calc_align(log_sys->buf_free,
						 OS_FILE_LOG_BLOCK_SIZE);

			ut_memmove(log_sys->buf, log_sys->buf + move_start,
				   move_end - move_start);
			log_sys->buf_free -= move_start;

			log_sys->buf_next_to_write -= move_start;
		}

		return(LOG_UNLOCK_FLUSH_LOCK);
	}

	return(0);
}

/******************************************************//**
Completes an i/o to a log file. */
UNIV_INTERN
void
log_io_complete(
/*============*/
	log_group_t*	group)	/*!< in: log group or a dummy pointer */
{
	ulint	unlock;

	if ((ulint) group & 0x1UL) {
		/* It was a checkpoint write */
		group = (log_group_t*)((ulint) group - 1);

		if (srv_unix_file_flush_method != SRV_UNIX_O_DSYNC
		    && srv_unix_file_flush_method != SRV_UNIX_NOSYNC) {

			fil_flush(group->space_id);
		}

		DBUG_PRINT("ib_log", ("checkpoint info written to group %u",
				      unsigned(group->id)));
		log_io_complete_checkpoint();

		return;
	}

	ut_error;	/*!< We currently use synchronous writing of the
			logs and cannot end up here! */

	if (srv_unix_file_flush_method != SRV_UNIX_O_DSYNC
	    && srv_unix_file_flush_method != SRV_UNIX_NOSYNC
	    && srv_flush_log_at_trx_commit != 2) {

		fil_flush(group->space_id);
	}

	mutex_enter(&(log_sys->mutex));
	ut_ad(!recv_no_log_write);

	ut_a(group->n_pending_writes > 0);
	ut_a(log_sys->n_pending_writes > 0);

	group->n_pending_writes--;
	log_sys->n_pending_writes--;
	MONITOR_DEC(MONITOR_PENDING_LOG_WRITE);

	unlock = log_group_check_flush_completion(group);
	unlock = unlock | log_sys_check_flush_completion();

	log_flush_do_unlocks(unlock);

	mutex_exit(&(log_sys->mutex));
}

/******************************************************//**
Writes a log file header to a log file space. */
static
void
log_group_file_header_flush(
/*========================*/
	log_group_t*	group,		/*!< in: log group */
	ulint		nth_file,	/*!< in: header to the nth file in the
					log file space */
	lsn_t		start_lsn)	/*!< in: log file data starts at this
					lsn */
{
	byte*	buf;
	lsn_t	dest_offset;

	ut_ad(mutex_own(&(log_sys->mutex)));
	ut_ad(!recv_no_log_write);
	ut_a(nth_file < group->n_files);

	buf = *(group->file_header_bufs + nth_file);

	mach_write_to_4(buf + LOG_GROUP_ID, group->id);
	mach_write_to_8(buf + LOG_FILE_START_LSN, start_lsn);

	/* Wipe over possible label of ibbackup --restore */
	memcpy(buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP, "    ", 4);

	dest_offset = nth_file * group->file_size;

	DBUG_PRINT("ib_log", ("write file %u header for group %u",
			      unsigned(nth_file), unsigned(group->id)));

	if (log_do_write) {
		log_sys->n_log_ios++;

		MONITOR_INC(MONITOR_LOG_IO);

		srv_stats.os_log_pending_writes.inc();

		fil_io(OS_FILE_WRITE | OS_FILE_LOG, true, group->space_id, 0,
		       (ulint) (dest_offset / UNIV_PAGE_SIZE),
		       (ulint) (dest_offset % UNIV_PAGE_SIZE),
		       OS_FILE_LOG_BLOCK_SIZE,
		       buf, group);

		srv_stats.os_log_pending_writes.dec();
	}
}

/******************************************************//**
Stores a 4-byte checksum to the trailer checksum field of a log block
before writing it to a log file. This checksum is used in recovery to
check the consistency of a log block. */
static
void
log_block_store_checksum(
/*=====================*/
	byte*	block)	/*!< in/out: pointer to a log block */
{
	log_block_set_checksum(block, log_block_calc_checksum(block));
}

/******************************************************//**
Writes a buffer to a log file group. */
UNIV_INTERN
void
log_group_write_buf(
/*================*/
	log_group_t*	group,		/*!< in: log group */
	byte*		buf,		/*!< in: buffer */
	ulint		len,		/*!< in: buffer len; must be divisible
					by OS_FILE_LOG_BLOCK_SIZE */
	lsn_t		start_lsn,	/*!< in: start lsn of the buffer; must
					be divisible by
					OS_FILE_LOG_BLOCK_SIZE */
	ulint		new_data_offset)/*!< in: start offset of new data in
					buf: this parameter is used to decide
					if we have to write a new log file
					header */
{
	ulint		write_len;
	ibool		write_header;
	lsn_t		next_offset;
	ulint		i;

	ut_ad(mutex_own(&(log_sys->mutex)));
	ut_ad(!recv_no_log_write);
	ut_a(len % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_a(start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

	if (new_data_offset == 0) {
		write_header = TRUE;
	} else {
		write_header = FALSE;
	}
loop:
	if (len == 0) {

		return;
	}

	next_offset = log_group_calc_lsn_offset(start_lsn, group);

	if ((next_offset % group->file_size == LOG_FILE_HDR_SIZE)
	    && write_header) {
		/* We start to write a new log file instance in the group */

		ut_a(next_offset / group->file_size <= ULINT_MAX);

		log_group_file_header_flush(group, (ulint)
					    (next_offset / group->file_size),
					    start_lsn);
		srv_stats.os_log_written.add(OS_FILE_LOG_BLOCK_SIZE);

		srv_stats.log_writes.inc();
	}

	if ((next_offset % group->file_size) + len > group->file_size) {

		/* if the above condition holds, then the below expression
		is < len which is ulint, so the typecast is ok */
		write_len = (ulint)
			(group->file_size - (next_offset % group->file_size));
	} else {
		write_len = len;
	}

	DBUG_PRINT("ib_log",
		   ("write " LSN_PF " to " LSN_PF
		    ": group %u len %u blocks %u..%u",
		    start_lsn, next_offset,
		    unsigned(group->id), unsigned(write_len),
		    unsigned(log_block_get_hdr_no(buf)),
		    unsigned(log_block_get_hdr_no(
				     buf + write_len
				     - OS_FILE_LOG_BLOCK_SIZE))));

	ut_ad(log_block_get_hdr_no(buf)
	      == log_block_convert_lsn_to_no(start_lsn));

	/* Calculate the checksums for each log block and write them to
	the trailer fields of the log blocks */

	for (i = 0; i < write_len / OS_FILE_LOG_BLOCK_SIZE; i++) {
		ut_ad(log_block_get_hdr_no(
			      buf + i * OS_FILE_LOG_BLOCK_SIZE)
		      == log_block_get_hdr_no(buf) + i);
		log_block_store_checksum(buf + i * OS_FILE_LOG_BLOCK_SIZE);
	}

	if (log_do_write) {
		log_sys->n_log_ios++;

		MONITOR_INC(MONITOR_LOG_IO);

		srv_stats.os_log_pending_writes.inc();

		ut_a(next_offset / UNIV_PAGE_SIZE <= ULINT_MAX);

		fil_io(OS_FILE_WRITE | OS_FILE_LOG, true, group->space_id, 0,
		       (ulint) (next_offset / UNIV_PAGE_SIZE),
		       (ulint) (next_offset % UNIV_PAGE_SIZE), write_len, buf,
		       group);

		srv_stats.os_log_pending_writes.dec();

		srv_stats.os_log_written.add(write_len);
		srv_stats.log_writes.inc();
	}

	if (write_len < len) {
		start_lsn += write_len;
		len -= write_len;
		buf += write_len;

		write_header = TRUE;

		goto loop;
	}
}

/******************************************************//**
This function is called, e.g., when a transaction wants to commit. It checks
that the log has been written to the log file up to the last log entry written
by the transaction. If there is a flush running, it waits and checks if the
flush flushed enough. If not, starts a new flush. */
UNIV_INTERN
void
log_write_up_to(
/*============*/
	lsn_t	lsn,	/*!< in: log sequence number up to which
			the log should be written,
			LSN_MAX if not specified */
	ulint	wait,	/*!< in: LOG_NO_WAIT, LOG_WAIT_ONE_GROUP,
			or LOG_WAIT_ALL_GROUPS */
	ibool	flush_to_disk)
			/*!< in: TRUE if we want the written log
			also to be flushed to disk */
{
	log_group_t*	group;
	ulint		start_offset;
	ulint		end_offset;
	ulint		area_start;
	ulint		area_end;
#ifdef UNIV_DEBUG
	ulint		loop_count	= 0;
#endif /* UNIV_DEBUG */
	ulint		unlock;

	ut_ad(!srv_read_only_mode);

	if (recv_no_ibuf_operations) {
		/* Recovery is running and no operations on the log files are
		allowed yet (the variable name .._no_ibuf_.. is misleading) */

		return;
	}

loop:
#ifdef UNIV_DEBUG
	loop_count++;

	ut_ad(loop_count < 5);

# if 0
	if (loop_count > 2) {
		fprintf(stderr, "Log loop count %lu\n", loop_count);
	}
# endif
#endif

	mutex_enter(&(log_sys->mutex));
	ut_ad(!recv_no_log_write);

	if (flush_to_disk
	    && log_sys->flushed_to_disk_lsn >= lsn) {

		mutex_exit(&(log_sys->mutex));

		return;
	}

	if (!flush_to_disk
	    && (log_sys->written_to_all_lsn >= lsn
		|| (log_sys->written_to_some_lsn >= lsn
		    && wait != LOG_WAIT_ALL_GROUPS))) {

		mutex_exit(&(log_sys->mutex));

		return;
	}

	if (log_sys->n_pending_writes > 0) {
		/* A write (+ possibly flush to disk) is running */

		if (flush_to_disk
		    && log_sys->current_flush_lsn >= lsn) {
			/* The write + flush will write enough: wait for it to
			complete */

			goto do_waits;
		}

		if (!flush_to_disk
		    && log_sys->write_lsn >= lsn) {
			/* The write will write enough: wait for it to
			complete */

			goto do_waits;
		}

		mutex_exit(&(log_sys->mutex));

		/* Wait for the write to complete and try to start a new
		write */

		os_event_wait(log_sys->no_flush_event);

		goto loop;
	}

	if (!flush_to_disk
	    && log_sys->buf_free == log_sys->buf_next_to_write) {
		/* Nothing to write and no flush to disk requested */

		mutex_exit(&(log_sys->mutex));

		return;
	}

	DBUG_PRINT("ib_log", ("write " LSN_PF " to " LSN_PF,
			      log_sys->written_to_all_lsn,
			      log_sys->lsn));
	log_sys->n_pending_writes++;
	MONITOR_INC(MONITOR_PENDING_LOG_WRITE);

	group = UT_LIST_GET_FIRST(log_sys->log_groups);
	group->n_pending_writes++;	/*!< We assume here that we have only
					one log group! */

	os_event_reset(log_sys->no_flush_event);
	os_event_reset(log_sys->one_flushed_event);

	start_offset = log_sys->buf_next_to_write;
	end_offset = log_sys->buf_free;

	area_start = ut_calc_align_down(start_offset, OS_FILE_LOG_BLOCK_SIZE);
	area_end = ut_calc_align(end_offset, OS_FILE_LOG_BLOCK_SIZE);

	ut_ad(area_end - area_start > 0);

	log_sys->write_lsn = log_sys->lsn;

	if (flush_to_disk) {
		log_sys->current_flush_lsn = log_sys->lsn;
	}

	log_sys->one_flushed = FALSE;

	log_block_set_flush_bit(log_sys->buf + area_start, TRUE);
	log_block_set_checkpoint_no(
		log_sys->buf + area_end - OS_FILE_LOG_BLOCK_SIZE,
		log_sys->next_checkpoint_no);

	/* Copy the last, incompletely written, log block a log block length
	up, so that when the flush operation writes from the log buffer, the
	segment to write will not be changed by writers to the log */

	ut_memcpy(log_sys->buf + area_end,
		  log_sys->buf + area_end - OS_FILE_LOG_BLOCK_SIZE,
		  OS_FILE_LOG_BLOCK_SIZE);

	log_sys->buf_free += OS_FILE_LOG_BLOCK_SIZE;
	log_sys->write_end_offset = log_sys->buf_free;

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	/* Do the write to the log files */

	while (group) {
		log_group_write_buf(
			group, log_sys->buf + area_start,
			area_end - area_start,
			ut_uint64_align_down(log_sys->written_to_all_lsn,
					     OS_FILE_LOG_BLOCK_SIZE),
			start_offset - area_start);

		log_group_set_fields(group, log_sys->write_lsn);

		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	mutex_exit(&(log_sys->mutex));

	if (srv_unix_file_flush_method == SRV_UNIX_O_DSYNC) {
		/* O_DSYNC means the OS did not buffer the log file at all:
		so we have also flushed to disk what we have written */

		log_sys->flushed_to_disk_lsn = log_sys->write_lsn;

	} else if (flush_to_disk) {

		group = UT_LIST_GET_FIRST(log_sys->log_groups);

		fil_flush(group->space_id);
		log_sys->flushed_to_disk_lsn = log_sys->write_lsn;
	}

	mutex_enter(&(log_sys->mutex));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	ut_a(group->n_pending_writes == 1);
	ut_a(log_sys->n_pending_writes == 1);

	group->n_pending_writes--;
	log_sys->n_pending_writes--;
	MONITOR_DEC(MONITOR_PENDING_LOG_WRITE);

	unlock = log_group_check_flush_completion(group);
	unlock = unlock | log_sys_check_flush_completion();

	log_flush_do_unlocks(unlock);

	mutex_exit(&(log_sys->mutex));

	return;

do_waits:
	mutex_exit(&(log_sys->mutex));

	switch (wait) {
	case LOG_WAIT_ONE_GROUP:
		os_event_wait(log_sys->one_flushed_event);
		break;
	case LOG_WAIT_ALL_GROUPS:
		os_event_wait(log_sys->no_flush_event);
		break;
#ifdef UNIV_DEBUG
	case LOG_NO_WAIT:
		break;
	default:
		ut_error;
#endif /* UNIV_DEBUG */
	}
}

/****************************************************************//**
Does a syncronous flush of the log buffer to disk. */
UNIV_INTERN
void
log_buffer_flush_to_disk(void)
/*==========================*/
{
	lsn_t	lsn;

	ut_ad(!srv_read_only_mode);
	mutex_enter(&(log_sys->mutex));

	lsn = log_sys->lsn;

	mutex_exit(&(log_sys->mutex));

	log_write_up_to(lsn, LOG_WAIT_ALL_GROUPS, TRUE);
}

/****************************************************************//**
This functions writes the log buffer to the log file and if 'flush'
is set it forces a flush of the log file as well. This is meant to be
called from background master thread only as it does not wait for
the write (+ possible flush) to finish. */
UNIV_INTERN
void
log_buffer_sync_in_background(
/*==========================*/
	ibool	flush)	/*!< in: flush the logs to disk */
{
	lsn_t	lsn;

	mutex_enter(&(log_sys->mutex));

	lsn = log_sys->lsn;

	mutex_exit(&(log_sys->mutex));

	log_write_up_to(lsn, LOG_NO_WAIT, flush);
}

/********************************************************************

Tries to establish a big enough margin of free space in the log buffer, such
that a new log entry can be catenated without an immediate need for a flush. */
static
void
log_flush_margin(void)
/*==================*/
{
	log_t*	log	= log_sys;
	lsn_t	lsn	= 0;

	mutex_enter(&(log->mutex));

	if (log->buf_free > log->max_buf_free) {

		if (log->n_pending_writes > 0) {
			/* A flush is running: hope that it will provide enough
			free space */
		} else {
			lsn = log->lsn;
		}
	}

	mutex_exit(&(log->mutex));

	if (lsn) {
		log_write_up_to(lsn, LOG_NO_WAIT, FALSE);
	}
}

/****************************************************************//**
Advances the smallest lsn for which there are unflushed dirty blocks in the
buffer pool. NOTE: this function may only be called if the calling thread owns
no synchronization objects!
@return false if there was a flush batch of the same type running,
which means that we could not start this flush batch */
static
bool
log_preflush_pool_modified_pages(
/*=============================*/
	lsn_t	new_oldest)	/*!< in: try to advance oldest_modified_lsn
				at least to this lsn */
{
	bool	success;
	ulint	n_pages;

	if (recv_recovery_on) {
		/* If the recovery is running, we must first apply all
		log records to their respective file pages to get the
		right modify lsn values to these pages: otherwise, there
		might be pages on disk which are not yet recovered to the
		current lsn, and even after calling this function, we could
		not know how up-to-date the disk version of the database is,
		and we could not make a new checkpoint on the basis of the
		info on the buffer pool only. */

		recv_apply_hashed_log_recs(TRUE);
	}

	success = buf_flush_list(ULINT_MAX, new_oldest, &n_pages);

	buf_flush_wait_batch_end(NULL, BUF_FLUSH_LIST);

	if (!success) {
		MONITOR_INC(MONITOR_FLUSH_SYNC_WAITS);
	}

	MONITOR_INC_VALUE_CUMULATIVE(
		MONITOR_FLUSH_SYNC_TOTAL_PAGE,
		MONITOR_FLUSH_SYNC_COUNT,
		MONITOR_FLUSH_SYNC_PAGES,
		n_pages);

	return(success);
}

/******************************************************//**
Completes a checkpoint. */
static
void
log_complete_checkpoint(void)
/*=========================*/
{
	ut_ad(mutex_own(&(log_sys->mutex)));
	ut_ad(log_sys->n_pending_checkpoint_writes == 0);

	log_sys->next_checkpoint_no++;

	log_sys->last_checkpoint_lsn = log_sys->next_checkpoint_lsn;
	MONITOR_SET(MONITOR_LSN_CHECKPOINT_AGE,
		    log_sys->lsn - log_sys->last_checkpoint_lsn);

	rw_lock_x_unlock_gen(&(log_sys->checkpoint_lock), LOG_CHECKPOINT);
}

/******************************************************//**
Completes an asynchronous checkpoint info write i/o to a log file. */
static
void
log_io_complete_checkpoint(void)
/*============================*/
{
	mutex_enter(&(log_sys->mutex));

	ut_ad(log_sys->n_pending_checkpoint_writes > 0);

	log_sys->n_pending_checkpoint_writes--;
	MONITOR_DEC(MONITOR_PENDING_CHECKPOINT_WRITE);

	if (log_sys->n_pending_checkpoint_writes == 0) {
		log_complete_checkpoint();
	}

	mutex_exit(&(log_sys->mutex));
}

/*******************************************************************//**
Writes info to a checkpoint about a log group. */
static
void
log_checkpoint_set_nth_group_info(
/*==============================*/
	byte*	buf,	/*!< in: buffer for checkpoint info */
	ulint	n,	/*!< in: nth slot */
	ulint	file_no,/*!< in: archived file number */
	ulint	offset)	/*!< in: archived file offset */
{
	ut_ad(n < LOG_MAX_N_GROUPS);

	mach_write_to_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
			+ 8 * n + LOG_CHECKPOINT_ARCHIVED_FILE_NO, file_no);
	mach_write_to_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
			+ 8 * n + LOG_CHECKPOINT_ARCHIVED_OFFSET, offset);
}

/*******************************************************************//**
Gets info from a checkpoint about a log group. */
UNIV_INTERN
void
log_checkpoint_get_nth_group_info(
/*==============================*/
	const byte*	buf,	/*!< in: buffer containing checkpoint info */
	ulint		n,	/*!< in: nth slot */
	ulint*		file_no,/*!< out: archived file number */
	ulint*		offset)	/*!< out: archived file offset */
{
	ut_ad(n < LOG_MAX_N_GROUPS);

	*file_no = mach_read_from_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
				    + 8 * n + LOG_CHECKPOINT_ARCHIVED_FILE_NO);
	*offset = mach_read_from_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
				   + 8 * n + LOG_CHECKPOINT_ARCHIVED_OFFSET);
}

/******************************************************//**
Writes the checkpoint info to a log group header. */
static
void
log_group_checkpoint(
/*=================*/
	log_group_t*	group)	/*!< in: log group */
{
	log_group_t*	group2;
	lsn_t		lsn_offset;
	ulint		write_offset;
	ulint		fold;
	byte*		buf;
	ulint		i;

	ut_ad(!srv_read_only_mode);
	ut_ad(mutex_own(&(log_sys->mutex)));
#if LOG_CHECKPOINT_SIZE > OS_FILE_LOG_BLOCK_SIZE
# error "LOG_CHECKPOINT_SIZE > OS_FILE_LOG_BLOCK_SIZE"
#endif

	buf = group->checkpoint_buf;

	mach_write_to_8(buf + LOG_CHECKPOINT_NO, log_sys->next_checkpoint_no);
	mach_write_to_8(buf + LOG_CHECKPOINT_LSN, log_sys->next_checkpoint_lsn);

	lsn_offset = log_group_calc_lsn_offset(log_sys->next_checkpoint_lsn,
					       group);
	mach_write_to_4(buf + LOG_CHECKPOINT_OFFSET_LOW32,
			lsn_offset & 0xFFFFFFFFUL);
	mach_write_to_4(buf + LOG_CHECKPOINT_OFFSET_HIGH32,
			lsn_offset >> 32);

	mach_write_to_4(buf + LOG_CHECKPOINT_LOG_BUF_SIZE, log_sys->buf_size);

	mach_write_to_8(buf + LOG_CHECKPOINT_ARCHIVED_LSN, LSN_MAX);

	for (i = 0; i < LOG_MAX_N_GROUPS; i++) {
		log_checkpoint_set_nth_group_info(buf, i, 0, 0);
	}

	group2 = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group2) {
		log_checkpoint_set_nth_group_info(buf, group2->id, 0, 0);

		group2 = UT_LIST_GET_NEXT(log_groups, group2);
	}

	fold = ut_fold_binary(buf, LOG_CHECKPOINT_CHECKSUM_1);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_1, fold);

	fold = ut_fold_binary(buf + LOG_CHECKPOINT_LSN,
			      LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_2, fold);

	/* We alternate the physical place of the checkpoint info in the first
	log file */

	if ((log_sys->next_checkpoint_no & 1) == 0) {
		write_offset = LOG_CHECKPOINT_1;
	} else {
		write_offset = LOG_CHECKPOINT_2;
	}

	if (log_do_write) {
		if (log_sys->n_pending_checkpoint_writes == 0) {

			rw_lock_x_lock_gen(&(log_sys->checkpoint_lock),
					   LOG_CHECKPOINT);
		}

		log_sys->n_pending_checkpoint_writes++;
		MONITOR_INC(MONITOR_PENDING_CHECKPOINT_WRITE);

		log_sys->n_log_ios++;

		MONITOR_INC(MONITOR_LOG_IO);

		/* We send as the last parameter the group machine address
		added with 1, as we want to distinguish between a normal log
		file write and a checkpoint field write */

		fil_io(OS_FILE_WRITE | OS_FILE_LOG, false, group->space_id, 0,
		       write_offset / UNIV_PAGE_SIZE,
		       write_offset % UNIV_PAGE_SIZE,
		       OS_FILE_LOG_BLOCK_SIZE,
		       buf, ((byte*) group + 1));

		ut_ad(((ulint) group & 0x1UL) == 0);
	}
}
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_HOTBACKUP
/******************************************************//**
Writes info to a buffer of a log group when log files are created in
backup restoration. */
UNIV_INTERN
void
log_reset_first_header_and_checkpoint(
/*==================================*/
	byte*		hdr_buf,/*!< in: buffer which will be written to the
				start of the first log file */
	ib_uint64_t	start)	/*!< in: lsn of the start of the first log file;
				we pretend that there is a checkpoint at
				start + LOG_BLOCK_HDR_SIZE */
{
	ulint		fold;
	byte*		buf;
	ib_uint64_t	lsn;

	mach_write_to_4(hdr_buf + LOG_GROUP_ID, 0);
	mach_write_to_8(hdr_buf + LOG_FILE_START_LSN, start);

	lsn = start + LOG_BLOCK_HDR_SIZE;

	/* Write the label of ibbackup --restore */
	strcpy((char*) hdr_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
	       "ibbackup ");
	ut_sprintf_timestamp((char*) hdr_buf
			     + (LOG_FILE_WAS_CREATED_BY_HOT_BACKUP
				+ (sizeof "ibbackup ") - 1));
	buf = hdr_buf + LOG_CHECKPOINT_1;

	mach_write_to_8(buf + LOG_CHECKPOINT_NO, 0);
	mach_write_to_8(buf + LOG_CHECKPOINT_LSN, lsn);

	mach_write_to_4(buf + LOG_CHECKPOINT_OFFSET_LOW32,
			LOG_FILE_HDR_SIZE + LOG_BLOCK_HDR_SIZE);
	mach_write_to_4(buf + LOG_CHECKPOINT_OFFSET_HIGH32, 0);

	mach_write_to_4(buf + LOG_CHECKPOINT_LOG_BUF_SIZE, 2 * 1024 * 1024);

	mach_write_to_8(buf + LOG_CHECKPOINT_ARCHIVED_LSN, LSN_MAX);

	fold = ut_fold_binary(buf, LOG_CHECKPOINT_CHECKSUM_1);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_1, fold);

	fold = ut_fold_binary(buf + LOG_CHECKPOINT_LSN,
			      LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_2, fold);

	/* Starting from InnoDB-3.23.50, we should also write info on
	allocated size in the tablespace, but unfortunately we do not
	know it here */
}
#endif /* UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/******************************************************//**
Reads a checkpoint info from a log group header to log_sys->checkpoint_buf. */
UNIV_INTERN
void
log_group_read_checkpoint_info(
/*===========================*/
	log_group_t*	group,	/*!< in: log group */
	ulint		field)	/*!< in: LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2 */
{
	ut_ad(mutex_own(&(log_sys->mutex)));

	log_sys->n_log_ios++;

	MONITOR_INC(MONITOR_LOG_IO);

	fil_io(OS_FILE_READ | OS_FILE_LOG, true, group->space_id, 0,
	       field / UNIV_PAGE_SIZE, field % UNIV_PAGE_SIZE,
	       OS_FILE_LOG_BLOCK_SIZE, log_sys->checkpoint_buf, NULL);
}

/******************************************************//**
Writes checkpoint info to groups. */
UNIV_INTERN
void
log_groups_write_checkpoint_info(void)
/*==================================*/
{
	log_group_t*	group;

	ut_ad(mutex_own(&(log_sys->mutex)));

	if (!srv_read_only_mode) {
		for (group = UT_LIST_GET_FIRST(log_sys->log_groups);
		     group;
		     group = UT_LIST_GET_NEXT(log_groups, group)) {

			log_group_checkpoint(group);
		}
	}
}

/******************************************************//**
Makes a checkpoint. Note that this function does not flush dirty
blocks from the buffer pool: it only checks what is lsn of the oldest
modification in the pool, and writes information about the lsn in
log files. Use log_make_checkpoint_at to flush also the pool.
@return	TRUE if success, FALSE if a checkpoint write was already running */
UNIV_INTERN
ibool
log_checkpoint(
/*===========*/
	ibool	sync,		/*!< in: TRUE if synchronous operation is
				desired */
	ibool	write_always)	/*!< in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
{
	lsn_t	oldest_lsn;

	ut_ad(!srv_read_only_mode);

	if (recv_recovery_is_on()) {
		recv_apply_hashed_log_recs(TRUE);
	}

	if (srv_unix_file_flush_method != SRV_UNIX_NOSYNC) {
		fil_flush_file_spaces(FIL_TABLESPACE);
	}

	mutex_enter(&(log_sys->mutex));

	ut_ad(!recv_no_log_write);
	oldest_lsn = log_buf_pool_get_oldest_modification();

	mutex_exit(&(log_sys->mutex));

	/* Because log also contains headers and dummy log records,
	if the buffer pool contains no dirty buffers, oldest_lsn
	gets the value log_sys->lsn from the previous function,
	and we must make sure that the log is flushed up to that
	lsn. If there are dirty buffers in the buffer pool, then our
	write-ahead-logging algorithm ensures that the log has been flushed
	up to oldest_lsn. */

	log_write_up_to(oldest_lsn, LOG_WAIT_ALL_GROUPS, TRUE);

	mutex_enter(&(log_sys->mutex));

	if (!write_always
	    && log_sys->last_checkpoint_lsn >= oldest_lsn) {

		mutex_exit(&(log_sys->mutex));

		return(TRUE);
	}

	ut_ad(log_sys->flushed_to_disk_lsn >= oldest_lsn);

	if (log_sys->n_pending_checkpoint_writes > 0) {
		/* A checkpoint write is running */

		mutex_exit(&(log_sys->mutex));

		if (sync) {
			/* Wait for the checkpoint write to complete */
			rw_lock_s_lock(&(log_sys->checkpoint_lock));
			rw_lock_s_unlock(&(log_sys->checkpoint_lock));
		}

		return(FALSE);
	}

	log_sys->next_checkpoint_lsn = oldest_lsn;

	DBUG_PRINT("ib_log", ("checkpoint " LSN_PF " at " LSN_PF,
			      log_sys->next_checkpoint_no,
			      oldest_lsn));

	log_groups_write_checkpoint_info();

	MONITOR_INC(MONITOR_NUM_CHECKPOINT);

	mutex_exit(&(log_sys->mutex));

	if (sync) {
		/* Wait for the checkpoint write to complete */
		rw_lock_s_lock(&(log_sys->checkpoint_lock));
		rw_lock_s_unlock(&(log_sys->checkpoint_lock));
	}

	return(TRUE);
}

/****************************************************************//**
Makes a checkpoint at a given lsn or later. */
UNIV_INTERN
void
log_make_checkpoint_at(
/*===================*/
	lsn_t	lsn,		/*!< in: make a checkpoint at this or a
				later lsn, if LSN_MAX, makes
				a checkpoint at the latest lsn */
	ibool	write_always)	/*!< in: the function normally checks if
				the new checkpoint would have a
				greater lsn than the previous one: if
				not, then no physical write is done;
				by setting this parameter TRUE, a
				physical write will always be made to
				log files */
{
	/* Preflush pages synchronously */

	while (!log_preflush_pool_modified_pages(lsn)) {
		/* Flush as much as we can */
	}

	while (!log_checkpoint(TRUE, write_always)) {
		/* Force a checkpoint */
	}
}

/****************************************************************//**
Tries to establish a big enough margin of free space in the log groups, such
that a new log entry can be catenated without an immediate need for a
checkpoint. NOTE: this function may only be called if the calling thread
owns no synchronization objects! */
static
void
log_checkpoint_margin(void)
/*=======================*/
{
	log_t*		log		= log_sys;
	lsn_t		age;
	lsn_t		checkpoint_age;
	ib_uint64_t	advance;
	lsn_t		oldest_lsn;
	ibool		checkpoint_sync;
	ibool		do_checkpoint;
	bool		success;
loop:
	checkpoint_sync = FALSE;
	do_checkpoint = FALSE;
	advance = 0;

	mutex_enter(&(log->mutex));
	ut_ad(!recv_no_log_write);

	if (log->check_flush_or_checkpoint == FALSE) {
		mutex_exit(&(log->mutex));

		return;
	}

	oldest_lsn = log_buf_pool_get_oldest_modification();

	age = log->lsn - oldest_lsn;

	if (age > log->max_modified_age_sync) {

		/* A flush is urgent: we have to do a synchronous preflush */
		advance = 2 * (age - log->max_modified_age_sync);
	}

	checkpoint_age = log->lsn - log->last_checkpoint_lsn;

	if (checkpoint_age > log->max_checkpoint_age) {
		/* A checkpoint is urgent: we do it synchronously */

		checkpoint_sync = TRUE;

		do_checkpoint = TRUE;

	} else if (checkpoint_age > log->max_checkpoint_age_async) {
		/* A checkpoint is not urgent: do it asynchronously */

		do_checkpoint = TRUE;

		log->check_flush_or_checkpoint = FALSE;
	} else {
		log->check_flush_or_checkpoint = FALSE;
	}

	mutex_exit(&(log->mutex));

	if (advance) {
		lsn_t	new_oldest = oldest_lsn + advance;

		success = log_preflush_pool_modified_pages(new_oldest);

		/* If the flush succeeded, this thread has done its part
		and can proceed. If it did not succeed, there was another
		thread doing a flush at the same time. */
		if (!success) {
			mutex_enter(&(log->mutex));

			log->check_flush_or_checkpoint = TRUE;

			mutex_exit(&(log->mutex));
			goto loop;
		}
	}

	if (do_checkpoint) {
		log_checkpoint(checkpoint_sync, FALSE);

		if (checkpoint_sync) {

			goto loop;
		}
	}
}

/******************************************************//**
Reads a specified log segment to a buffer. */
UNIV_INTERN
void
log_group_read_log_seg(
/*===================*/
	ulint		type,		/*!< in: LOG_ARCHIVE or LOG_RECOVER */
	byte*		buf,		/*!< in: buffer where to read */
	log_group_t*	group,		/*!< in: log group */
	lsn_t		start_lsn,	/*!< in: read area start */
	lsn_t		end_lsn)	/*!< in: read area end */
{
	ulint	len;
	lsn_t	source_offset;
	bool	sync;

	ut_ad(mutex_own(&(log_sys->mutex)));

	sync = (type == LOG_RECOVER);
loop:
	source_offset = log_group_calc_lsn_offset(start_lsn, group);

	ut_a(end_lsn - start_lsn <= ULINT_MAX);
	len = (ulint) (end_lsn - start_lsn);

	ut_ad(len != 0);

	if ((source_offset % group->file_size) + len > group->file_size) {

		/* If the above condition is true then len (which is ulint)
		is > the expression below, so the typecast is ok */
		len = (ulint) (group->file_size -
			(source_offset % group->file_size));
	}

	log_sys->n_log_ios++;

	MONITOR_INC(MONITOR_LOG_IO);

	ut_a(source_offset / UNIV_PAGE_SIZE <= ULINT_MAX);

	fil_io(OS_FILE_READ | OS_FILE_LOG, sync, group->space_id, 0,
	       (ulint) (source_offset / UNIV_PAGE_SIZE),
	       (ulint) (source_offset % UNIV_PAGE_SIZE),
	       len, buf, NULL);

	start_lsn += len;
	buf += len;

	if (start_lsn != end_lsn) {

		goto loop;
	}
}

/********************************************************************//**
Checks that there is enough free space in the log to start a new query step.
Flushes the log buffer or makes a new checkpoint if necessary. NOTE: this
function may only be called if the calling thread owns no synchronization
objects! */
UNIV_INTERN
void
log_check_margins(void)
/*===================*/
{
loop:
	log_flush_margin();

	log_checkpoint_margin();

	mutex_enter(&(log_sys->mutex));
	ut_ad(!recv_no_log_write);

	if (log_sys->check_flush_or_checkpoint) {

		mutex_exit(&(log_sys->mutex));

		goto loop;
	}

	mutex_exit(&(log_sys->mutex));
}

/****************************************************************//**
Makes a checkpoint at the latest lsn and writes it to first page of each
data file in the database, so that we know that the file spaces contain
all modifications up to that lsn. This can only be called at database
shutdown. This function also writes all log in log files to the log archive. */
UNIV_INTERN
void
logs_empty_and_mark_files_at_shutdown(void)
/*=======================================*/
{
	lsn_t			lsn;
	ulint			arch_log_no;
	ulint			count = 0;
	ulint			total_trx;
	ulint			pending_io;
	enum srv_thread_type	active_thd;
	const char*		thread_name;
	ibool			server_busy;

	ib_logf(IB_LOG_LEVEL_INFO, "Starting shutdown...");

	/* Wait until the master thread and all other operations are idle: our
	algorithm only works if the server is idle at shutdown */

	srv_shutdown_state = SRV_SHUTDOWN_CLEANUP;
loop:
	os_thread_sleep(100000);

	count++;

	/* We need the monitor threads to stop before we proceed with
	a shutdown. */

	thread_name = srv_any_background_threads_are_active();

	if (thread_name != NULL) {
		/* Print a message every 60 seconds if we are waiting
		for the monitor thread to exit. Master and worker
		threads check will be done later. */

		if (srv_print_verbose_log && count > 600) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Waiting for %s to exit", thread_name);
			count = 0;
		}

		goto loop;
	}

	/* Check that there are no longer transactions, except for
	PREPARED ones. We need this wait even for the 'very fast'
	shutdown, because the InnoDB layer may have committed or
	prepared transactions and we don't want to lose them. */

	total_trx = trx_sys_any_active_transactions();

	if (total_trx > 0) {

		if (srv_print_verbose_log && count > 600) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Waiting for %lu active transactions to finish",
				(ulong) total_trx);

			count = 0;
		}

		goto loop;
	}

	/* Check that the background threads are suspended */

	active_thd = srv_get_active_thread_type();

	if (active_thd != SRV_NONE) {

		if (active_thd == SRV_PURGE) {
			srv_purge_wakeup();
		}

		/* The srv_lock_timeout_thread, srv_error_monitor_thread
		and srv_monitor_thread should already exit by now. The
		only threads to be suspended are the master threads
		and worker threads (purge threads). Print the thread
		type if any of such threads not in suspended mode */
		if (srv_print_verbose_log && count > 600) {
			const char*	thread_type = "<null>";

			switch (active_thd) {
			case SRV_NONE:
				/* This shouldn't happen because we've
				already checked for this case before
				entering the if(). We handle it here
				to avoid a compiler warning. */
				ut_error;
			case SRV_WORKER:
				thread_type = "worker threads";
				break;
			case SRV_MASTER:
				thread_type = "master thread";
				break;
			case SRV_PURGE:
				thread_type = "purge thread";
				break;
			}

			ib_logf(IB_LOG_LEVEL_INFO,
				"Waiting for %s to be suspended",
				thread_type);
			count = 0;
		}

		goto loop;
	}

	/* At this point only page_cleaner should be active. We wait
	here to let it complete the flushing of the buffer pools
	before proceeding further. */
	srv_shutdown_state = SRV_SHUTDOWN_FLUSH_PHASE;
	count = 0;
	while (buf_page_cleaner_is_active) {
		++count;
		os_thread_sleep(100000);
		if (srv_print_verbose_log && count > 600) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Waiting for page_cleaner to "
				"finish flushing of buffer pool");
			count = 0;
		}
	}

	mutex_enter(&log_sys->mutex);
	server_busy = log_sys->n_pending_checkpoint_writes
		      || log_sys->n_pending_writes;
	mutex_exit(&log_sys->mutex);

	if (server_busy) {
		if (srv_print_verbose_log && count > 600) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Pending checkpoint_writes: %lu. "
				"Pending log flush writes: %lu",
				(ulong) log_sys->n_pending_checkpoint_writes,
				(ulong) log_sys->n_pending_writes);
			count = 0;
		}
		goto loop;
	}

	pending_io = buf_pool_check_no_pending_io();

	if (pending_io) {
		if (srv_print_verbose_log && count > 600) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Waiting for %lu buffer page I/Os to complete",
				(ulong) pending_io);
			count = 0;
		}

		goto loop;
	}

	if (srv_fast_shutdown == 2) {
		if (!srv_read_only_mode) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"MySQL has requested a very fast shutdown "
				"without flushing the InnoDB buffer pool to "
				"data files. At the next mysqld startup "
				"InnoDB will do a crash recovery!");

			/* In this fastest shutdown we do not flush the
			buffer pool:

			it is essentially a 'crash' of the InnoDB server.
			Make sure that the log is all flushed to disk, so
			that we can recover all committed transactions in
			a crash recovery. We must not write the lsn stamps
			to the data files, since at a startup InnoDB deduces
			from the stamps if the previous shutdown was clean. */

			log_buffer_flush_to_disk();

			/* Check that the background threads stay suspended */
			thread_name = srv_any_background_threads_are_active();

			if (thread_name != NULL) {
				ib_logf(IB_LOG_LEVEL_WARN,
					"Background thread %s woke up "
					"during shutdown", thread_name);
				goto loop;
			}
		}

		srv_shutdown_state = SRV_SHUTDOWN_LAST_PHASE;

		fil_close_all_files();

		thread_name = srv_any_background_threads_are_active();

		ut_a(!thread_name);

		return;
	}

	if (!srv_read_only_mode) {
		log_make_checkpoint_at(LSN_MAX, TRUE);
	}

	mutex_enter(&log_sys->mutex);

	lsn = log_sys->lsn;

	if (lsn != log_sys->last_checkpoint_lsn
	    ) {

		mutex_exit(&log_sys->mutex);

		goto loop;
	}

	arch_log_no = 0;

	mutex_exit(&log_sys->mutex);

	/* Check that the background threads stay suspended */
	thread_name = srv_any_background_threads_are_active();
	if (thread_name != NULL) {
		ib_logf(IB_LOG_LEVEL_WARN,
			"Background thread %s woke up during shutdown",
			thread_name);

		goto loop;
	}

	if (!srv_read_only_mode) {
		fil_flush_file_spaces(FIL_TABLESPACE);
		fil_flush_file_spaces(FIL_LOG);
	}

	/* The call fil_write_flushed_lsn_to_data_files() will pass the buffer
	pool: therefore it is essential that the buffer pool has been
	completely flushed to disk! (We do not call fil_write... if the
	'very fast' shutdown is enabled.) */

	if (!buf_all_freed()) {

		if (srv_print_verbose_log && count > 600) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Waiting for dirty buffer pages to be flushed");
			count = 0;
		}

		goto loop;
	}

	srv_shutdown_state = SRV_SHUTDOWN_LAST_PHASE;

	/* Make some checks that the server really is quiet */
	srv_thread_type	type = srv_get_active_thread_type();
	ut_a(type == SRV_NONE);

	bool	freed = buf_all_freed();
	ut_a(freed);

	ut_a(lsn == log_sys->lsn);

	if (lsn < srv_start_lsn) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Log sequence number at shutdown " LSN_PF " "
			"is lower than at startup " LSN_PF "!",
			lsn, srv_start_lsn);
	}

	srv_shutdown_lsn = lsn;

	if (!srv_read_only_mode) {
		fil_write_flushed_lsn_to_data_files(lsn, arch_log_no);

		fil_flush_file_spaces(FIL_TABLESPACE);
	}

	fil_close_all_files();

	/* Make some checks that the server really is quiet */
	type = srv_get_active_thread_type();
	ut_a(type == SRV_NONE);

	freed = buf_all_freed();
	ut_a(freed);

	ut_a(lsn == log_sys->lsn);
}

#ifdef UNIV_LOG_DEBUG
/******************************************************//**
Checks by parsing that the catenated log segment for a single mtr is
consistent. */
UNIV_INTERN
ibool
log_check_log_recs(
/*===============*/
	const byte*	buf,		/*!< in: pointer to the start of
					the log segment in the
					log_sys->buf log buffer */
	ulint		len,		/*!< in: segment length in bytes */
	ib_uint64_t	buf_start_lsn)	/*!< in: buffer start lsn */
{
	ib_uint64_t	contiguous_lsn;
	ib_uint64_t	scanned_lsn;
	const byte*	start;
	const byte*	end;
	byte*		buf1;
	byte*		scan_buf;

	ut_ad(mutex_own(&(log_sys->mutex)));

	if (len == 0) {

		return(TRUE);
	}

	start = ut_align_down(buf, OS_FILE_LOG_BLOCK_SIZE);
	end = ut_align(buf + len, OS_FILE_LOG_BLOCK_SIZE);

	buf1 = mem_alloc((end - start) + OS_FILE_LOG_BLOCK_SIZE);
	scan_buf = ut_align(buf1, OS_FILE_LOG_BLOCK_SIZE);

	ut_memcpy(scan_buf, start, end - start);

	recv_scan_log_recs((buf_pool_get_n_pages()
			   - (recv_n_pool_free_frames * srv_buf_pool_instances))
			   * UNIV_PAGE_SIZE, FALSE, scan_buf, end - start,
			   ut_uint64_align_down(buf_start_lsn,
						OS_FILE_LOG_BLOCK_SIZE),
			   &contiguous_lsn, &scanned_lsn);

	ut_a(scanned_lsn == buf_start_lsn + len);
	ut_a(recv_sys->recovered_lsn == scanned_lsn);

	mem_free(buf1);

	return(TRUE);
}
#endif /* UNIV_LOG_DEBUG */

/******************************************************//**
Peeks the current lsn.
@return	TRUE if success, FALSE if could not get the log system mutex */
UNIV_INTERN
ibool
log_peek_lsn(
/*=========*/
	lsn_t*	lsn)	/*!< out: if returns TRUE, current lsn is here */
{
	if (0 == mutex_enter_nowait(&(log_sys->mutex))) {
		*lsn = log_sys->lsn;

		mutex_exit(&(log_sys->mutex));

		return(TRUE);
	}

	return(FALSE);
}

/******************************************************//**
Prints info of the log. */
UNIV_INTERN
void
log_print(
/*======*/
	FILE*	file)	/*!< in: file where to print */
{
	double	time_elapsed;
	time_t	current_time;

	mutex_enter(&(log_sys->mutex));

	fprintf(file,
		"Log sequence number " LSN_PF "\n"
		"Log flushed up to   " LSN_PF "\n"
		"Pages flushed up to " LSN_PF "\n"
		"Last checkpoint at  " LSN_PF "\n",
		log_sys->lsn,
		log_sys->flushed_to_disk_lsn,
		log_buf_pool_get_oldest_modification(),
		log_sys->last_checkpoint_lsn);

	current_time = time(NULL);

	time_elapsed = difftime(current_time,
				log_sys->last_printout_time);

	if (time_elapsed <= 0) {
		time_elapsed = 1;
	}

	fprintf(file,
		"%lu pending log writes, %lu pending chkp writes\n"
		"%lu log i/o's done, %.2f log i/o's/second\n",
		(ulong) log_sys->n_pending_writes,
		(ulong) log_sys->n_pending_checkpoint_writes,
		(ulong) log_sys->n_log_ios,
		((double)(log_sys->n_log_ios - log_sys->n_log_ios_old)
		 / time_elapsed));

	log_sys->n_log_ios_old = log_sys->n_log_ios;
	log_sys->last_printout_time = current_time;

	mutex_exit(&(log_sys->mutex));
}

/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
UNIV_INTERN
void
log_refresh_stats(void)
/*===================*/
{
	log_sys->n_log_ios_old = log_sys->n_log_ios;
	log_sys->last_printout_time = time(NULL);
}

/********************************************************//**
Closes a log group. */
static
void
log_group_close(
/*===========*/
	log_group_t*	group)		/* in,own: log group to close */
{
	ulint	i;

	for (i = 0; i < group->n_files; i++) {
		mem_free(group->file_header_bufs_ptr[i]);
	}

	mem_free(group->file_header_bufs_ptr);
	mem_free(group->file_header_bufs);
	mem_free(group->checkpoint_buf_ptr);
	mem_free(group);
}

/********************************************************//**
Closes all log groups. */
UNIV_INTERN
void
log_group_close_all(void)
/*=====================*/
{
	log_group_t*	group;

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (UT_LIST_GET_LEN(log_sys->log_groups) > 0) {
		log_group_t*	prev_group = group;

		group = UT_LIST_GET_NEXT(log_groups, group);
		UT_LIST_REMOVE(log_groups, log_sys->log_groups, prev_group);

		log_group_close(prev_group);
	}
}

/********************************************************//**
Shutdown the log system but do not release all the memory. */
UNIV_INTERN
void
log_shutdown(void)
/*==============*/
{
	log_group_close_all();

	mem_free(log_sys->buf_ptr);
	log_sys->buf_ptr = NULL;
	log_sys->buf = NULL;
	mem_free(log_sys->checkpoint_buf_ptr);
	log_sys->checkpoint_buf_ptr = NULL;
	log_sys->checkpoint_buf = NULL;

	os_event_free(log_sys->no_flush_event);
	os_event_free(log_sys->one_flushed_event);

	rw_lock_free(&log_sys->checkpoint_lock);

	mutex_free(&log_sys->mutex);

#ifdef UNIV_LOG_DEBUG
	recv_sys_debug_free();
#endif

	recv_sys_close();
}

/********************************************************//**
Free the log system data structures. */
UNIV_INTERN
void
log_mem_free(void)
/*==============*/
{
	if (log_sys != NULL) {
		recv_sys_mem_free();
		mem_free(log_sys);

		log_sys = NULL;
	}
}
#endif /* !UNIV_HOTBACKUP */
