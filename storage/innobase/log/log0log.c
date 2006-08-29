/******************************************************
Database log

(c) 1995-1997 Innobase Oy

Created 12/9/1995 Heikki Tuuri
*******************************************************/

#include "log0log.h"

#ifdef UNIV_NONINL
#include "log0log.ic"
#endif

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

/* Current free limit of space 0; protected by the log sys mutex; 0 means
uninitialized */
ulint	log_fsp_current_free_limit		= 0;

/* Global log system variable */
log_t*	log_sys	= NULL;

#ifdef UNIV_DEBUG
ibool	log_do_write = TRUE;

ibool	log_debug_writes = FALSE;
#endif /* UNIV_DEBUG */

/* These control how often we print warnings if the last checkpoint is too
old */
ibool	log_has_printed_chkp_warning = FALSE;
time_t	log_last_warning_time;

#ifdef UNIV_LOG_ARCHIVE
/* Pointer to this variable is used as the i/o-message when we do i/o to an
archive */
byte	log_archive_io;
#endif /* UNIV_LOG_ARCHIVE */

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

/**********************************************************
Completes a checkpoint write i/o to a log file. */
static
void
log_io_complete_checkpoint(void);
/*============================*/
#ifdef UNIV_LOG_ARCHIVE
/**********************************************************
Completes an archiving i/o. */
static
void
log_io_complete_archive(void);
/*=========================*/
#endif /* UNIV_LOG_ARCHIVE */

/********************************************************************
Sets the global variable log_fsp_current_free_limit. Also makes a checkpoint,
so that we know that the limit has been written to a log checkpoint field
on disk. */

void
log_fsp_current_free_limit_set_and_checkpoint(
/*==========================================*/
	ulint	limit)	/* in: limit to set */
{
	ibool	success;

	mutex_enter(&(log_sys->mutex));

	log_fsp_current_free_limit = limit;

	mutex_exit(&(log_sys->mutex));

	/* Try to make a synchronous checkpoint */

	success = FALSE;

	while (!success) {
		success = log_checkpoint(TRUE, TRUE);
	}
}

/********************************************************************
Returns the oldest modified block lsn in the pool, or log_sys->lsn if none
exists. */
static
dulint
log_buf_pool_get_oldest_modification(void)
/*======================================*/
{
	dulint	lsn;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	lsn = buf_pool_get_oldest_modification();

	if (ut_dulint_is_zero(lsn)) {

		lsn = log_sys->lsn;
	}

	return(lsn);
}

/****************************************************************
Opens the log for log_write_low. The log must be closed with log_close and
released with log_release. */

dulint
log_reserve_and_open(
/*=================*/
			/* out: start lsn of the log record */
	ulint	len)	/* in: length of data to be catenated */
{
	log_t*	log			= log_sys;
	ulint	len_upper_limit;
#ifdef UNIV_LOG_ARCHIVE
	ulint	archived_lsn_age;
	ulint	dummy;
#endif /* UNIV_LOG_ARCHIVE */
#ifdef UNIV_DEBUG
	ulint	count			= 0;
#endif /* UNIV_DEBUG */

	ut_a(len < log->buf_size / 2);
loop:
	mutex_enter(&(log->mutex));

	/* Calculate an upper limit for the space the string may take in the
	log buffer */

	len_upper_limit = LOG_BUF_WRITE_MARGIN + (5 * len) / 4;

	if (log->buf_free + len_upper_limit > log->buf_size) {

		mutex_exit(&(log->mutex));

		/* Not enough free space, do a syncronous flush of the log
		buffer */

		log_buffer_flush_to_disk();

		srv_log_waits++;

		ut_ad(++count < 50);

		goto loop;
	}

#ifdef UNIV_LOG_ARCHIVE
	if (log->archiving_state != LOG_ARCH_OFF) {

		archived_lsn_age = ut_dulint_minus(log->lsn,
							log->archived_lsn);
		if (archived_lsn_age + len_upper_limit
						> log->max_archived_lsn_age) {
			/* Not enough free archived space in log groups: do a
			synchronous archive write batch: */

			mutex_exit(&(log->mutex));

			ut_ad(len_upper_limit <= log->max_archived_lsn_age);

			log_archive_do(TRUE, &dummy);

			ut_ad(++count < 50);

			goto loop;
		}
	}
#endif /* UNIV_LOG_ARCHIVE */

#ifdef UNIV_LOG_DEBUG
	log->old_buf_free = log->buf_free;
	log->old_lsn = log->lsn;
#endif
	return(log->lsn);
}

/****************************************************************
Writes to the log the string given. It is assumed that the caller holds the
log mutex. */

void
log_write_low(
/*==========*/
	byte*	str,		/* in: string */
	ulint	str_len)	/* in: string length */
{
	log_t*	log	= log_sys;
	ulint	len;
	ulint	data_len;
	byte*	log_block;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log->mutex)));
#endif /* UNIV_SYNC_DEBUG */
part_loop:
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

	log_block = ut_align_down(log->buf + log->buf_free,
						OS_FILE_LOG_BLOCK_SIZE);
	log_block_set_data_len(log_block, data_len);

	if (data_len == OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {
		/* This block became full */
		log_block_set_data_len(log_block, OS_FILE_LOG_BLOCK_SIZE);
		log_block_set_checkpoint_no(log_block,
						log_sys->next_checkpoint_no);
		len += LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE;

		log->lsn = ut_dulint_add(log->lsn, len);

		/* Initialize the next block header */
		log_block_init(log_block + OS_FILE_LOG_BLOCK_SIZE, log->lsn);
	} else {
		log->lsn = ut_dulint_add(log->lsn, len);
	}

	log->buf_free += len;

	ut_ad(log->buf_free <= log->buf_size);

	if (str_len > 0) {
		goto part_loop;
	}

	srv_log_write_requests++;
}

/****************************************************************
Closes the log. */

dulint
log_close(void)
/*===========*/
			/* out: lsn */
{
	byte*	log_block;
	ulint	first_rec_group;
	dulint	oldest_lsn;
	dulint	lsn;
	log_t*	log	= log_sys;
	ulint	checkpoint_age;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	lsn = log->lsn;

	log_block = ut_align_down(log->buf + log->buf_free,
						OS_FILE_LOG_BLOCK_SIZE);
	first_rec_group = log_block_get_first_rec_group(log_block);

	if (first_rec_group == 0) {
		/* We initialized a new log block which was not written
		full by the current mtr: the next mtr log record group
		will start within this block at the offset data_len */

		log_block_set_first_rec_group(log_block,
					log_block_get_data_len(log_block));
	}

	if (log->buf_free > log->max_buf_free) {

		log->check_flush_or_checkpoint = TRUE;
	}

	checkpoint_age = ut_dulint_minus(lsn, log->last_checkpoint_lsn);

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
			fprintf(stderr,
"  InnoDB: ERROR: the age of the last checkpoint is %lu,\n"
"InnoDB: which exceeds the log group capacity %lu.\n"
"InnoDB: If you are using big BLOB or TEXT rows, you must set the\n"
"InnoDB: combined size of log files at least 10 times bigger than the\n"
"InnoDB: largest such row.\n",
				(ulong) checkpoint_age,
				(ulong) log->log_group_capacity);
		}
	}

	if (checkpoint_age <= log->max_modified_age_async) {

		goto function_exit;
	}

	oldest_lsn = buf_pool_get_oldest_modification();

	if (ut_dulint_is_zero(oldest_lsn)
		|| (ut_dulint_minus(lsn, oldest_lsn)
			> log->max_modified_age_async)
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

#ifdef UNIV_LOG_ARCHIVE
/**********************************************************
Pads the current log block full with dummy log records. Used in producing
consistent archived log files. */
static
void
log_pad_current_log_block(void)
/*===========================*/
{
	byte	b		= MLOG_DUMMY_RECORD;
	ulint	pad_length;
	ulint	i;
	dulint	lsn;

	/* We retrieve lsn only because otherwise gcc crashed on HP-UX */
	lsn = log_reserve_and_open(OS_FILE_LOG_BLOCK_SIZE);

	pad_length = OS_FILE_LOG_BLOCK_SIZE
			- (log_sys->buf_free % OS_FILE_LOG_BLOCK_SIZE)
			- LOG_BLOCK_TRL_SIZE;

	for (i = 0; i < pad_length; i++) {
		log_write_low(&b, 1);
	}

	lsn = log_sys->lsn;

	log_close();
	log_release();

	ut_a((ut_dulint_get_low(lsn) % OS_FILE_LOG_BLOCK_SIZE)
						== LOG_BLOCK_HDR_SIZE);
}
#endif /* UNIV_LOG_ARCHIVE */

/**********************************************************
Calculates the data capacity of a log group, when the log file headers are not
included. */

ulint
log_group_get_capacity(
/*===================*/
				/* out: capacity in bytes */
	log_group_t*	group)	/* in: log group */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	return((group->file_size - LOG_FILE_HDR_SIZE) * group->n_files);
}

/**********************************************************
Calculates the offset within a log group, when the log file headers are not
included. */
UNIV_INLINE
ulint
log_group_calc_size_offset(
/*=======================*/
				/* out: size offset (<= offset) */
	ulint		offset,	/* in: real offset within the log group */
	log_group_t*	group)	/* in: log group */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	return(offset - LOG_FILE_HDR_SIZE * (1 + offset / group->file_size));
}

/**********************************************************
Calculates the offset within a log group, when the log file headers are
included. */
UNIV_INLINE
ulint
log_group_calc_real_offset(
/*=======================*/
				/* out: real offset (>= offset) */
	ulint		offset,	/* in: size offset within the log group */
	log_group_t*	group)	/* in: log group */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	return(offset + LOG_FILE_HDR_SIZE
		* (1 + offset / (group->file_size - LOG_FILE_HDR_SIZE)));
}

/**********************************************************
Calculates the offset of an lsn within a log group. */
static
ulint
log_group_calc_lsn_offset(
/*======================*/
				/* out: offset within the log group */
	dulint		lsn,	/* in: lsn, must be within 4 GB of
				group->lsn */
	log_group_t*	group)	/* in: log group */
{
	dulint		gr_lsn;
	ib_longlong	gr_lsn_size_offset;
	ib_longlong	difference;
	ib_longlong	group_size;
	ib_longlong	offset;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	/* If total log file size is > 2 GB we can easily get overflows
	with 32-bit integers. Use 64-bit integers instead. */

	gr_lsn = group->lsn;

	gr_lsn_size_offset = (ib_longlong)
		log_group_calc_size_offset(group->lsn_offset, group);

	group_size = (ib_longlong) log_group_get_capacity(group);

	if (ut_dulint_cmp(lsn, gr_lsn) >= 0) {

		difference = (ib_longlong) ut_dulint_minus(lsn, gr_lsn);
	} else {
		difference = (ib_longlong) ut_dulint_minus(gr_lsn, lsn);

		difference = difference % group_size;

		difference = group_size - difference;
	}

	offset = (gr_lsn_size_offset + difference) % group_size;

	ut_a(offset < (((ib_longlong) 1) << 32)); /* offset must be < 4 GB */

	/* fprintf(stderr,
		"Offset is %lu gr_lsn_offset is %lu difference is %lu\n",
		(ulint)offset,(ulint)gr_lsn_size_offset, (ulint)difference);
	*/

	return(log_group_calc_real_offset((ulint)offset, group));
}

/***********************************************************************
Calculates where in log files we find a specified lsn. */

ulint
log_calc_where_lsn_is(
/*==================*/
						/* out: log file number */
	ib_longlong*	log_file_offset,	/* out: offset in that file
						(including the header) */
	dulint		first_header_lsn,	/* in: first log file start
						lsn */
	dulint		lsn,			/* in: lsn whose position to
						determine */
	ulint		n_log_files,		/* in: total number of log
						files */
	ib_longlong	log_file_size)		/* in: log file size
						(including the header) */
{
	ib_longlong	ib_lsn;
	ib_longlong	ib_first_header_lsn;
	ib_longlong	capacity	= log_file_size - LOG_FILE_HDR_SIZE;
	ulint		file_no;
	ib_longlong	add_this_many;

	ib_lsn = ut_conv_dulint_to_longlong(lsn);
	ib_first_header_lsn = ut_conv_dulint_to_longlong(first_header_lsn);

	if (ib_lsn < ib_first_header_lsn) {
		add_this_many = 1 + (ib_first_header_lsn - ib_lsn)
				/ (capacity * (ib_longlong)n_log_files);
		ib_lsn += add_this_many
			  * capacity * (ib_longlong)n_log_files;
	}

	ut_a(ib_lsn >= ib_first_header_lsn);

	file_no = ((ulint)((ib_lsn - ib_first_header_lsn) / capacity))
			  % n_log_files;
	*log_file_offset = (ib_lsn - ib_first_header_lsn) % capacity;

	*log_file_offset = *log_file_offset + LOG_FILE_HDR_SIZE;

	return(file_no);
}

/************************************************************
Sets the field values in group to correspond to a given lsn. For this function
to work, the values must already be correctly initialized to correspond to
some lsn, for instance, a checkpoint lsn. */

void
log_group_set_fields(
/*=================*/
	log_group_t*	group,	/* in: group */
	dulint		lsn)	/* in: lsn for which the values should be
				set */
{
	group->lsn_offset = log_group_calc_lsn_offset(lsn, group);
	group->lsn = lsn;
}

/*********************************************************************
Calculates the recommended highest values for lsn - last_checkpoint_lsn,
lsn - buf_get_oldest_modification(), and lsn - max_archive_lsn_age. */
static
ibool
log_calc_max_ages(void)
/*===================*/
			/* out: error value FALSE if the smallest log group is
			too small to accommodate the number of OS threads in
			the database server */
{
	log_group_t*	group;
	ulint		margin;
	ulint		free;
	ibool		success		= TRUE;
	ulint		smallest_capacity;
	ulint		archive_margin;
	ulint		smallest_archive_margin;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	mutex_enter(&(log_sys->mutex));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	ut_ad(group);

	smallest_capacity = ULINT_MAX;
	smallest_archive_margin = ULINT_MAX;

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
		success = FALSE;

		goto failure;
	} else {
		margin = smallest_capacity - free;
	}

	margin = ut_min(margin, log_sys->adm_checkpoint_interval);

	margin = margin - margin / 10;	/* Add still some extra safety */

	log_sys->log_group_capacity = smallest_capacity;

	log_sys->max_modified_age_async = margin
				- margin / LOG_POOL_PREFLUSH_RATIO_ASYNC;
	log_sys->max_modified_age_sync = margin
				- margin / LOG_POOL_PREFLUSH_RATIO_SYNC;

	log_sys->max_checkpoint_age_async = margin - margin
					/ LOG_POOL_CHECKPOINT_RATIO_ASYNC;
	log_sys->max_checkpoint_age = margin;

#ifdef UNIV_LOG_ARCHIVE
	log_sys->max_archived_lsn_age = smallest_archive_margin;

	log_sys->max_archived_lsn_age_async = smallest_archive_margin
						- smallest_archive_margin /
						  LOG_ARCHIVE_RATIO_ASYNC;
#endif /* UNIV_LOG_ARCHIVE */
failure:
	mutex_exit(&(log_sys->mutex));

	if (!success) {
		fprintf(stderr,
"InnoDB: Error: ib_logfiles are too small for innodb_thread_concurrency %lu.\n"
"InnoDB: The combined size of ib_logfiles should be bigger than\n"
"InnoDB: 200 kB * innodb_thread_concurrency.\n"
"InnoDB: To get mysqld to start up, set innodb_thread_concurrency in my.cnf\n"
"InnoDB: to a lower value, for example, to 8. After an ERROR-FREE shutdown\n"
"InnoDB: of mysqld you can adjust the size of ib_logfiles, as explained in\n"
"InnoDB: http://dev.mysql.com/doc/refman/5.0/en/adding-and-removing.html\n"
"InnoDB: Cannot continue operation. Calling exit(1).\n",
			(ulong)srv_thread_concurrency);

		exit(1);
	}

	return(success);
}

/**********************************************************
Initializes the log. */

void
log_init(void)
/*==========*/
{
	byte*	buf;

	log_sys = mem_alloc(sizeof(log_t));

	mutex_create(&log_sys->mutex, SYNC_LOG);

	mutex_enter(&(log_sys->mutex));

	/* Start the lsn from one log block from zero: this way every
	log record has a start lsn != zero, a fact which we will use */

	log_sys->lsn = LOG_START_LSN;

	ut_a(LOG_BUFFER_SIZE >= 16 * OS_FILE_LOG_BLOCK_SIZE);
	ut_a(LOG_BUFFER_SIZE >= 4 * UNIV_PAGE_SIZE);

	buf = ut_malloc(LOG_BUFFER_SIZE + OS_FILE_LOG_BLOCK_SIZE);
	log_sys->buf = ut_align(buf, OS_FILE_LOG_BLOCK_SIZE);

	log_sys->buf_size = LOG_BUFFER_SIZE;

	memset(log_sys->buf, '\0', LOG_BUFFER_SIZE);

	log_sys->max_buf_free = log_sys->buf_size / LOG_BUF_FLUSH_RATIO
				- LOG_BUF_FLUSH_MARGIN;
	log_sys->check_flush_or_checkpoint = TRUE;
	UT_LIST_INIT(log_sys->log_groups);

	log_sys->n_log_ios = 0;

	log_sys->n_log_ios_old = log_sys->n_log_ios;
	log_sys->last_printout_time = time(NULL);
	/*----------------------------*/

	log_sys->buf_next_to_write = 0;

	log_sys->write_lsn = ut_dulint_zero;
	log_sys->current_flush_lsn = ut_dulint_zero;
	log_sys->flushed_to_disk_lsn = ut_dulint_zero;

	log_sys->written_to_some_lsn = log_sys->lsn;
	log_sys->written_to_all_lsn = log_sys->lsn;

	log_sys->n_pending_writes = 0;

	log_sys->no_flush_event = os_event_create(NULL);

	os_event_set(log_sys->no_flush_event);

	log_sys->one_flushed_event = os_event_create(NULL);

	os_event_set(log_sys->one_flushed_event);

	/*----------------------------*/
	log_sys->adm_checkpoint_interval = ULINT_MAX;

	log_sys->next_checkpoint_no = ut_dulint_zero;
	log_sys->last_checkpoint_lsn = log_sys->lsn;
	log_sys->n_pending_checkpoint_writes = 0;

	rw_lock_create(&log_sys->checkpoint_lock, SYNC_NO_ORDER_CHECK);

	log_sys->checkpoint_buf = ut_align(
				mem_alloc(2 * OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE);
	memset(log_sys->checkpoint_buf, '\0', OS_FILE_LOG_BLOCK_SIZE);
	/*----------------------------*/

#ifdef UNIV_LOG_ARCHIVE
	/* Under MySQL, log archiving is always off */
	log_sys->archiving_state = LOG_ARCH_OFF;
	log_sys->archived_lsn = log_sys->lsn;
	log_sys->next_archived_lsn = ut_dulint_zero;

	log_sys->n_pending_archive_ios = 0;

	rw_lock_create(&log_sys->archive_lock, SYNC_NO_ORDER_CHECK);

	log_sys->archive_buf = NULL;

			/* ut_align(
				ut_malloc(LOG_ARCHIVE_BUF_SIZE
					  + OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE); */
	log_sys->archive_buf_size = 0;

	/* memset(log_sys->archive_buf, '\0', LOG_ARCHIVE_BUF_SIZE); */

	log_sys->archiving_on = os_event_create(NULL);
#endif /* UNIV_LOG_ARCHIVE */

	/*----------------------------*/

	log_block_init(log_sys->buf, log_sys->lsn);
	log_block_set_first_rec_group(log_sys->buf, LOG_BLOCK_HDR_SIZE);

	log_sys->buf_free = LOG_BLOCK_HDR_SIZE;
	log_sys->lsn = ut_dulint_add(LOG_START_LSN, LOG_BLOCK_HDR_SIZE);

	mutex_exit(&(log_sys->mutex));

#ifdef UNIV_LOG_DEBUG
	recv_sys_create();
	recv_sys_init(FALSE, buf_pool_get_curr_size());

	recv_sys->parse_start_lsn = log_sys->lsn;
	recv_sys->scanned_lsn = log_sys->lsn;
	recv_sys->scanned_checkpoint_no = 0;
	recv_sys->recovered_lsn = log_sys->lsn;
	recv_sys->limit_lsn = ut_dulint_max;
#endif
}

/**********************************************************************
Inits a log group to the log system. */

void
log_group_init(
/*===========*/
	ulint	id,			/* in: group id */
	ulint	n_files,		/* in: number of log files */
	ulint	file_size,		/* in: log file size in bytes */
	ulint	space_id,		/* in: space id of the file space
					which contains the log files of this
					group */
	ulint	archive_space_id __attribute__((unused)))
					/* in: space id of the file space
					which contains some archived log
					files for this group; currently, only
					for the first log group this is
					used */
{
	ulint	i;

	log_group_t*	group;

	group = mem_alloc(sizeof(log_group_t));

	group->id = id;
	group->n_files = n_files;
	group->file_size = file_size;
	group->space_id = space_id;
	group->state = LOG_GROUP_OK;
	group->lsn = LOG_START_LSN;
	group->lsn_offset = LOG_FILE_HDR_SIZE;
	group->n_pending_writes = 0;

	group->file_header_bufs = mem_alloc(sizeof(byte*) * n_files);
#ifdef UNIV_LOG_ARCHIVE
	group->archive_file_header_bufs = mem_alloc(sizeof(byte*) * n_files);
#endif /* UNIV_LOG_ARCHIVE */

	for (i = 0; i < n_files; i++) {
		*(group->file_header_bufs + i) = ut_align(
			mem_alloc(LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE);

		memset(*(group->file_header_bufs + i), '\0',
							LOG_FILE_HDR_SIZE);

#ifdef UNIV_LOG_ARCHIVE
		*(group->archive_file_header_bufs + i) = ut_align(
			mem_alloc(LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE);
		memset(*(group->archive_file_header_bufs + i), '\0',
							LOG_FILE_HDR_SIZE);
#endif /* UNIV_LOG_ARCHIVE */
	}

#ifdef UNIV_LOG_ARCHIVE
	group->archive_space_id = archive_space_id;

	group->archived_file_no = 0;
	group->archived_offset = 0;
#endif /* UNIV_LOG_ARCHIVE */

	group->checkpoint_buf = ut_align(
				mem_alloc(2 * OS_FILE_LOG_BLOCK_SIZE),
						OS_FILE_LOG_BLOCK_SIZE);

	memset(group->checkpoint_buf, '\0', OS_FILE_LOG_BLOCK_SIZE);

	UT_LIST_ADD_LAST(log_groups, log_sys->log_groups, group);

	ut_a(log_calc_max_ages());
}

/**********************************************************************
Does the unlockings needed in flush i/o completion. */
UNIV_INLINE
void
log_flush_do_unlocks(
/*=================*/
	ulint	code)	/* in: any ORed combination of LOG_UNLOCK_FLUSH_LOCK
			and LOG_UNLOCK_NONE_FLUSHED_LOCK */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

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

/**********************************************************************
Checks if a flush is completed for a log group and does the completion
routine if yes. */
UNIV_INLINE
ulint
log_group_check_flush_completion(
/*=============================*/
				/* out: LOG_UNLOCK_NONE_FLUSHED_LOCK or 0 */
	log_group_t*	group)	/* in: log group */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (!log_sys->one_flushed && group->n_pending_writes == 0) {
#ifdef UNIV_DEBUG
		if (log_debug_writes) {
			fprintf(stderr,
				"Log flushed first to group %lu\n", (ulong) group->id);
		}
#endif /* UNIV_DEBUG */
		log_sys->written_to_some_lsn = log_sys->write_lsn;
		log_sys->one_flushed = TRUE;

		return(LOG_UNLOCK_NONE_FLUSHED_LOCK);
	}

#ifdef UNIV_DEBUG
	if (log_debug_writes && (group->n_pending_writes == 0)) {

		fprintf(stderr, "Log flushed to group %lu\n", (ulong) group->id);
	}
#endif /* UNIV_DEBUG */
	return(0);
}

/**********************************************************
Checks if a flush is completed and does the completion routine if yes. */
static
ulint
log_sys_check_flush_completion(void)
/*================================*/
			/* out: LOG_UNLOCK_FLUSH_LOCK or 0 */
{
	ulint	move_start;
	ulint	move_end;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

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

/**********************************************************
Completes an i/o to a log file. */

void
log_io_complete(
/*============*/
	log_group_t*	group)	/* in: log group or a dummy pointer */
{
	ulint	unlock;

#ifdef UNIV_LOG_ARCHIVE
	if ((byte*)group == &log_archive_io) {
		/* It was an archive write */

		log_io_complete_archive();

		return;
	}
#endif /* UNIV_LOG_ARCHIVE */

	if ((ulint)group & 0x1UL) {
		/* It was a checkpoint write */
		group = (log_group_t*)((ulint)group - 1);

		if (srv_unix_file_flush_method != SRV_UNIX_O_DSYNC
		   && srv_unix_file_flush_method != SRV_UNIX_NOSYNC) {

			fil_flush(group->space_id);
		}

#ifdef UNIV_DEBUG
		if (log_debug_writes) {
			fprintf(stderr,
				"Checkpoint info written to group %lu\n",
				group->id);
		}
#endif /* UNIV_DEBUG */
		log_io_complete_checkpoint();

		return;
	}

	ut_error;	/* We currently use synchronous writing of the
			logs and cannot end up here! */

	if (srv_unix_file_flush_method != SRV_UNIX_O_DSYNC
		&& srv_unix_file_flush_method != SRV_UNIX_NOSYNC
		&& srv_flush_log_at_trx_commit != 2) {

		fil_flush(group->space_id);
	}

	mutex_enter(&(log_sys->mutex));

	ut_a(group->n_pending_writes > 0);
	ut_a(log_sys->n_pending_writes > 0);

	group->n_pending_writes--;
	log_sys->n_pending_writes--;

	unlock = log_group_check_flush_completion(group);
	unlock = unlock | log_sys_check_flush_completion();

	log_flush_do_unlocks(unlock);

	mutex_exit(&(log_sys->mutex));
}

/**********************************************************
Writes a log file header to a log file space. */
static
void
log_group_file_header_flush(
/*========================*/
	log_group_t*	group,		/* in: log group */
	ulint		nth_file,	/* in: header to the nth file in the
					log file space */
	dulint		start_lsn)	/* in: log file data starts at this
					lsn */
{
	byte*	buf;
	ulint	dest_offset;
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	ut_a(nth_file < group->n_files);

	buf = *(group->file_header_bufs + nth_file);

	mach_write_to_4(buf + LOG_GROUP_ID, group->id);
	mach_write_to_8(buf + LOG_FILE_START_LSN, start_lsn);

	/* Wipe over possible label of ibbackup --restore */
	memcpy(buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP, "    ", 4);

	dest_offset = nth_file * group->file_size;

#ifdef UNIV_DEBUG
	if (log_debug_writes) {
		fprintf(stderr,
			"Writing log file header to group %lu file %lu\n",
			(ulong) group->id, (ulong) nth_file);
	}
#endif /* UNIV_DEBUG */
	if (log_do_write) {
		log_sys->n_log_ios++;

		srv_os_log_pending_writes++;

		fil_io(OS_FILE_WRITE | OS_FILE_LOG, TRUE, group->space_id,
				dest_offset / UNIV_PAGE_SIZE,
				dest_offset % UNIV_PAGE_SIZE,
				OS_FILE_LOG_BLOCK_SIZE,
				buf, group);

		srv_os_log_pending_writes--;
	}
}

/**********************************************************
Stores a 4-byte checksum to the trailer checksum field of a log block
before writing it to a log file. This checksum is used in recovery to
check the consistency of a log block. */
static
void
log_block_store_checksum(
/*=====================*/
	byte*	block)	/* in/out: pointer to a log block */
{
	log_block_set_checksum(block, log_block_calc_checksum(block));
}

/**********************************************************
Writes a buffer to a log file group. */

void
log_group_write_buf(
/*================*/
	log_group_t*	group,		/* in: log group */
	byte*		buf,		/* in: buffer */
	ulint		len,		/* in: buffer len; must be divisible
					by OS_FILE_LOG_BLOCK_SIZE */
	dulint		start_lsn,	/* in: start lsn of the buffer; must
					be divisible by
					OS_FILE_LOG_BLOCK_SIZE */
	ulint		new_data_offset)/* in: start offset of new data in
					buf: this parameter is used to decide
					if we have to write a new log file
					header */
{
	ulint	write_len;
	ibool	write_header;
	ulint	next_offset;
	ulint	i;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(len % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_a(ut_dulint_get_low(start_lsn) % OS_FILE_LOG_BLOCK_SIZE == 0);

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

		log_group_file_header_flush(group,
				next_offset / group->file_size, start_lsn);
		srv_os_log_written+= OS_FILE_LOG_BLOCK_SIZE;
		srv_log_writes++;
	}

	if ((next_offset % group->file_size) + len > group->file_size) {

		write_len = group->file_size
					- (next_offset % group->file_size);
	} else {
		write_len = len;
	}

#ifdef UNIV_DEBUG
	if (log_debug_writes) {

		fprintf(stderr,
		"Writing log file segment to group %lu offset %lu len %lu\n"
		"start lsn %lu %lu\n"
		"First block n:o %lu last block n:o %lu\n",
			(ulong) group->id, (ulong) next_offset,
			(ulong) write_len,
			(ulong) ut_dulint_get_high(start_lsn),
			(ulong) ut_dulint_get_low(start_lsn),
			(ulong) log_block_get_hdr_no(buf),
			(ulong) log_block_get_hdr_no(
				buf + write_len - OS_FILE_LOG_BLOCK_SIZE));
		ut_a(log_block_get_hdr_no(buf)
			== log_block_convert_lsn_to_no(start_lsn));

		for (i = 0; i < write_len / OS_FILE_LOG_BLOCK_SIZE; i++) {

			ut_a(log_block_get_hdr_no(buf) + i
				== log_block_get_hdr_no(buf
					+ i * OS_FILE_LOG_BLOCK_SIZE));
		}
	}
#endif /* UNIV_DEBUG */
	/* Calculate the checksums for each log block and write them to
	the trailer fields of the log blocks */

	for (i = 0; i < write_len / OS_FILE_LOG_BLOCK_SIZE; i++) {
		log_block_store_checksum(buf + i * OS_FILE_LOG_BLOCK_SIZE);
	}

	if (log_do_write) {
		log_sys->n_log_ios++;

		srv_os_log_pending_writes++;

		fil_io(OS_FILE_WRITE | OS_FILE_LOG, TRUE, group->space_id,
			next_offset / UNIV_PAGE_SIZE,
			next_offset % UNIV_PAGE_SIZE, write_len, buf, group);

		srv_os_log_pending_writes--;

		srv_os_log_written+= write_len;
		srv_log_writes++;
	}

	if (write_len < len) {
		start_lsn = ut_dulint_add(start_lsn, write_len);
		len -= write_len;
		buf += write_len;

		write_header = TRUE;

		goto loop;
	}
}

/**********************************************************
This function is called, e.g., when a transaction wants to commit. It checks
that the log has been written to the log file up to the last log entry written
by the transaction. If there is a flush running, it waits and checks if the
flush flushed enough. If not, starts a new flush. */

void
log_write_up_to(
/*============*/
	dulint	lsn,	/* in: log sequence number up to which the log should
			be written, ut_dulint_max if not specified */
	ulint	wait,	/* in: LOG_NO_WAIT, LOG_WAIT_ONE_GROUP,
			or LOG_WAIT_ALL_GROUPS */
	ibool	flush_to_disk)
			/* in: TRUE if we want the written log also to be
			flushed to disk */
{
	log_group_t*	group;
	ulint		start_offset;
	ulint		end_offset;
	ulint		area_start;
	ulint		area_end;
	ulint		loop_count;
	ulint		unlock;

	if (recv_no_ibuf_operations) {
		/* Recovery is running and no operations on the log files are
		allowed yet (the variable name .._no_ibuf_.. is misleading) */

		return;
	}

	loop_count = 0;
loop:
	loop_count++;

	ut_ad(loop_count < 5);

	if (loop_count > 2) {
/*		fprintf(stderr, "Log loop count %lu\n", loop_count); */
	}

	mutex_enter(&(log_sys->mutex));

	if (flush_to_disk
		&& ut_dulint_cmp(log_sys->flushed_to_disk_lsn, lsn) >= 0) {

		mutex_exit(&(log_sys->mutex));

		return;
	}

	if (!flush_to_disk
		&& (ut_dulint_cmp(log_sys->written_to_all_lsn, lsn) >= 0
			|| (ut_dulint_cmp(log_sys->written_to_some_lsn, lsn)
				>= 0
				&& wait != LOG_WAIT_ALL_GROUPS))) {

		mutex_exit(&(log_sys->mutex));

		return;
	}

	if (log_sys->n_pending_writes > 0) {
		/* A write (+ possibly flush to disk) is running */

		if (flush_to_disk
			&& ut_dulint_cmp(log_sys->current_flush_lsn, lsn)
			>= 0) {
			/* The write + flush will write enough: wait for it to
			complete  */

			goto do_waits;
		}

		if (!flush_to_disk
			&& ut_dulint_cmp(log_sys->write_lsn, lsn) >= 0) {
			/* The write will write enough: wait for it to
			complete  */

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

#ifdef UNIV_DEBUG
	if (log_debug_writes) {
		fprintf(stderr,
			"Writing log from %lu %lu up to lsn %lu %lu\n",
			(ulong) ut_dulint_get_high(log_sys->written_to_all_lsn),
			(ulong) ut_dulint_get_low(log_sys->written_to_all_lsn),
			(ulong) ut_dulint_get_high(log_sys->lsn),
			(ulong)	ut_dulint_get_low(log_sys->lsn));
	}
#endif /* UNIV_DEBUG */
	log_sys->n_pending_writes++;

	group = UT_LIST_GET_FIRST(log_sys->log_groups);
	group->n_pending_writes++;	/* We assume here that we have only
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
		log_group_write_buf(group,
			log_sys->buf + area_start,
			area_end - area_start,
			ut_dulint_align_down(log_sys->written_to_all_lsn,
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

	unlock = log_group_check_flush_completion(group);
	unlock = unlock | log_sys_check_flush_completion();

	log_flush_do_unlocks(unlock);

	mutex_exit(&(log_sys->mutex));

	return;

do_waits:
	mutex_exit(&(log_sys->mutex));

	if (wait == LOG_WAIT_ONE_GROUP) {
		os_event_wait(log_sys->one_flushed_event);
	} else if (wait == LOG_WAIT_ALL_GROUPS) {
		os_event_wait(log_sys->no_flush_event);
	} else {
		ut_ad(wait == LOG_NO_WAIT);
	}
}

/********************************************************************
Does a syncronous flush of the log buffer to disk. */

void
log_buffer_flush_to_disk(void)
/*==========================*/
{
	dulint	lsn;

	mutex_enter(&(log_sys->mutex));

	lsn = log_sys->lsn;

	mutex_exit(&(log_sys->mutex));

	log_write_up_to(lsn, LOG_WAIT_ALL_GROUPS, TRUE);
}

/********************************************************************
Tries to establish a big enough margin of free space in the log buffer, such
that a new log entry can be catenated without an immediate need for a flush. */
static
void
log_flush_margin(void)
/*==================*/
{
	ibool	do_flush	= FALSE;
	log_t*	log		= log_sys;
	dulint	lsn;

	mutex_enter(&(log->mutex));

	if (log->buf_free > log->max_buf_free) {

		if (log->n_pending_writes > 0) {
			/* A flush is running: hope that it will provide enough
			free space */
		} else {
			do_flush = TRUE;
			lsn = log->lsn;
		}
	}

	mutex_exit(&(log->mutex));

	if (do_flush) {
		log_write_up_to(lsn, LOG_NO_WAIT, FALSE);
	}
}

/********************************************************************
Advances the smallest lsn for which there are unflushed dirty blocks in the
buffer pool. NOTE: this function may only be called if the calling thread owns
no synchronization objects! */

ibool
log_preflush_pool_modified_pages(
/*=============================*/
				/* out: FALSE if there was a flush batch of
				the same type running, which means that we
				could not start this flush batch */
	dulint	new_oldest,	/* in: try to advance oldest_modified_lsn
				at least to this lsn */
	ibool	sync)		/* in: TRUE if synchronous operation is
				desired */
{
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

	n_pages = buf_flush_batch(BUF_FLUSH_LIST, ULINT_MAX, new_oldest);

	if (sync) {
		buf_flush_wait_batch_end(BUF_FLUSH_LIST);
	}

	if (n_pages == ULINT_UNDEFINED) {

		return(FALSE);
	}

	return(TRUE);
}

/**********************************************************
Completes a checkpoint. */
static
void
log_complete_checkpoint(void)
/*=========================*/
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(log_sys->n_pending_checkpoint_writes == 0);

	log_sys->next_checkpoint_no
			= ut_dulint_add(log_sys->next_checkpoint_no, 1);

	log_sys->last_checkpoint_lsn = log_sys->next_checkpoint_lsn;

	rw_lock_x_unlock_gen(&(log_sys->checkpoint_lock), LOG_CHECKPOINT);
}

/**********************************************************
Completes an asynchronous checkpoint info write i/o to a log file. */
static
void
log_io_complete_checkpoint(void)
/*============================*/
{
	mutex_enter(&(log_sys->mutex));

	ut_ad(log_sys->n_pending_checkpoint_writes > 0);

	log_sys->n_pending_checkpoint_writes--;

	if (log_sys->n_pending_checkpoint_writes == 0) {
		log_complete_checkpoint();
	}

	mutex_exit(&(log_sys->mutex));
}

/***********************************************************************
Writes info to a checkpoint about a log group. */
static
void
log_checkpoint_set_nth_group_info(
/*==============================*/
	byte*	buf,	/* in: buffer for checkpoint info */
	ulint	n,	/* in: nth slot */
	ulint	file_no,/* in: archived file number */
	ulint	offset)	/* in: archived file offset */
{
	ut_ad(n < LOG_MAX_N_GROUPS);

	mach_write_to_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
			+ 8 * n + LOG_CHECKPOINT_ARCHIVED_FILE_NO, file_no);
	mach_write_to_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
			+ 8 * n + LOG_CHECKPOINT_ARCHIVED_OFFSET, offset);
}

/***********************************************************************
Gets info from a checkpoint about a log group. */

void
log_checkpoint_get_nth_group_info(
/*==============================*/
	byte*	buf,	/* in: buffer containing checkpoint info */
	ulint	n,	/* in: nth slot */
	ulint*	file_no,/* out: archived file number */
	ulint*	offset)	/* out: archived file offset */
{
	ut_ad(n < LOG_MAX_N_GROUPS);

	*file_no = mach_read_from_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
				+ 8 * n + LOG_CHECKPOINT_ARCHIVED_FILE_NO);
	*offset = mach_read_from_4(buf + LOG_CHECKPOINT_GROUP_ARRAY
				+ 8 * n + LOG_CHECKPOINT_ARCHIVED_OFFSET);
}

/**********************************************************
Writes the checkpoint info to a log group header. */
static
void
log_group_checkpoint(
/*=================*/
	log_group_t*	group)	/* in: log group */
{
	log_group_t*	group2;
#ifdef UNIV_LOG_ARCHIVE
	dulint	archived_lsn;
	dulint	next_archived_lsn;
#endif /* UNIV_LOG_ARCHIVE */
	ulint	write_offset;
	ulint	fold;
	byte*	buf;
	ulint	i;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
#if LOG_CHECKPOINT_SIZE > OS_FILE_LOG_BLOCK_SIZE
# error "LOG_CHECKPOINT_SIZE > OS_FILE_LOG_BLOCK_SIZE"
#endif

	buf = group->checkpoint_buf;

	mach_write_to_8(buf + LOG_CHECKPOINT_NO, log_sys->next_checkpoint_no);
	mach_write_to_8(buf + LOG_CHECKPOINT_LSN,
						log_sys->next_checkpoint_lsn);

	mach_write_to_4(buf + LOG_CHECKPOINT_OFFSET,
			log_group_calc_lsn_offset(
					log_sys->next_checkpoint_lsn, group));

	mach_write_to_4(buf + LOG_CHECKPOINT_LOG_BUF_SIZE, log_sys->buf_size);

#ifdef UNIV_LOG_ARCHIVE
	if (log_sys->archiving_state == LOG_ARCH_OFF) {
		archived_lsn = ut_dulint_max;
	} else {
		archived_lsn = log_sys->archived_lsn;

		if (0 != ut_dulint_cmp(archived_lsn,
					log_sys->next_archived_lsn)) {
			next_archived_lsn = log_sys->next_archived_lsn;
			/* For debugging only */
		}
	}

	mach_write_to_8(buf + LOG_CHECKPOINT_ARCHIVED_LSN, archived_lsn);
#else /* UNIV_LOG_ARCHIVE */
	mach_write_to_8(buf + LOG_CHECKPOINT_ARCHIVED_LSN, ut_dulint_max);
#endif /* UNIV_LOG_ARCHIVE */

	for (i = 0; i < LOG_MAX_N_GROUPS; i++) {
		log_checkpoint_set_nth_group_info(buf, i, 0, 0);
	}

	group2 = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group2) {
		log_checkpoint_set_nth_group_info(buf, group2->id,
#ifdef UNIV_LOG_ARCHIVE
						group2->archived_file_no,
						group2->archived_offset
#else /* UNIV_LOG_ARCHIVE */
						0, 0
#endif /* UNIV_LOG_ARCHIVE */
						);

		group2 = UT_LIST_GET_NEXT(log_groups, group2);
	}

	fold = ut_fold_binary(buf, LOG_CHECKPOINT_CHECKSUM_1);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_1, fold);

	fold = ut_fold_binary(buf + LOG_CHECKPOINT_LSN,
			LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_2, fold);

	/* Starting from InnoDB-3.23.50, we also write info on allocated
	size in the tablespace */

	mach_write_to_4(buf + LOG_CHECKPOINT_FSP_FREE_LIMIT,
						log_fsp_current_free_limit);

	mach_write_to_4(buf + LOG_CHECKPOINT_FSP_MAGIC_N,
					LOG_CHECKPOINT_FSP_MAGIC_N_VAL);

	/* We alternate the physical place of the checkpoint info in the first
	log file */

	if (ut_dulint_get_low(log_sys->next_checkpoint_no) % 2 == 0) {
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

		log_sys->n_log_ios++;

		/* We send as the last parameter the group machine address
		added with 1, as we want to distinguish between a normal log
		file write and a checkpoint field write */

		fil_io(OS_FILE_WRITE | OS_FILE_LOG, FALSE, group->space_id,
				write_offset / UNIV_PAGE_SIZE,
				write_offset % UNIV_PAGE_SIZE,
				OS_FILE_LOG_BLOCK_SIZE,
				buf, ((byte*)group + 1));

		ut_ad(((ulint)group & 0x1UL) == 0);
	}
}

/**********************************************************
Writes info to a buffer of a log group when log files are created in
backup restoration. */

void
log_reset_first_header_and_checkpoint(
/*==================================*/
	byte*	hdr_buf,/* in: buffer which will be written to the start
			of the first log file */
	dulint	start)	/* in: lsn of the start of the first log file;
			we pretend that there is a checkpoint at
			start + LOG_BLOCK_HDR_SIZE */
{
	ulint	fold;
	byte*	buf;
	dulint	lsn;

	mach_write_to_4(hdr_buf + LOG_GROUP_ID, 0);
	mach_write_to_8(hdr_buf + LOG_FILE_START_LSN, start);

	lsn = ut_dulint_add(start, LOG_BLOCK_HDR_SIZE);

	/* Write the label of ibbackup --restore */
	strcpy((char*) hdr_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
				"ibbackup ");
	ut_sprintf_timestamp(
			(char*) hdr_buf + (LOG_FILE_WAS_CREATED_BY_HOT_BACKUP
						+ (sizeof "ibbackup ") - 1));
	buf = hdr_buf + LOG_CHECKPOINT_1;

	mach_write_to_8(buf + LOG_CHECKPOINT_NO, ut_dulint_zero);
	mach_write_to_8(buf + LOG_CHECKPOINT_LSN, lsn);

	mach_write_to_4(buf + LOG_CHECKPOINT_OFFSET,
				LOG_FILE_HDR_SIZE + LOG_BLOCK_HDR_SIZE);

	mach_write_to_4(buf + LOG_CHECKPOINT_LOG_BUF_SIZE, 2 * 1024 * 1024);

	mach_write_to_8(buf + LOG_CHECKPOINT_ARCHIVED_LSN, ut_dulint_max);

	fold = ut_fold_binary(buf, LOG_CHECKPOINT_CHECKSUM_1);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_1, fold);

	fold = ut_fold_binary(buf + LOG_CHECKPOINT_LSN,
			LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
	mach_write_to_4(buf + LOG_CHECKPOINT_CHECKSUM_2, fold);

	/* Starting from InnoDB-3.23.50, we should also write info on
	allocated size in the tablespace, but unfortunately we do not
	know it here */
}

/**********************************************************
Reads a checkpoint info from a log group header to log_sys->checkpoint_buf. */

void
log_group_read_checkpoint_info(
/*===========================*/
	log_group_t*	group,	/* in: log group */
	ulint		field)	/* in: LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2 */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	log_sys->n_log_ios++;

	fil_io(OS_FILE_READ | OS_FILE_LOG, TRUE, group->space_id,
			field / UNIV_PAGE_SIZE, field % UNIV_PAGE_SIZE,
			OS_FILE_LOG_BLOCK_SIZE, log_sys->checkpoint_buf, NULL);
}

/**********************************************************
Writes checkpoint info to groups. */

void
log_groups_write_checkpoint_info(void)
/*==================================*/
{
	log_group_t*	group;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		log_group_checkpoint(group);

		group = UT_LIST_GET_NEXT(log_groups, group);
	}
}

/**********************************************************
Makes a checkpoint. Note that this function does not flush dirty
blocks from the buffer pool: it only checks what is lsn of the oldest
modification in the pool, and writes information about the lsn in
log files. Use log_make_checkpoint_at to flush also the pool. */

ibool
log_checkpoint(
/*===========*/
				/* out: TRUE if success, FALSE if a checkpoint
				write was already running */
	ibool	sync,		/* in: TRUE if synchronous operation is
				desired */
	ibool	write_always)	/* in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
{
	dulint	oldest_lsn;

	if (recv_recovery_is_on()) {
		recv_apply_hashed_log_recs(TRUE);
	}

	if (srv_unix_file_flush_method != SRV_UNIX_NOSYNC) {
		fil_flush_file_spaces(FIL_TABLESPACE);
	}

	mutex_enter(&(log_sys->mutex));

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

	if (!write_always && ut_dulint_cmp(
			log_sys->last_checkpoint_lsn, oldest_lsn) >= 0) {

		mutex_exit(&(log_sys->mutex));

		return(TRUE);
	}

	ut_ad(ut_dulint_cmp(log_sys->written_to_all_lsn, oldest_lsn) >= 0);

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

#ifdef UNIV_DEBUG
	if (log_debug_writes) {
		fprintf(stderr, "Making checkpoint no %lu at lsn %lu %lu\n",
			(ulong) ut_dulint_get_low(log_sys->next_checkpoint_no),
			(ulong) ut_dulint_get_high(oldest_lsn),
			(ulong) ut_dulint_get_low(oldest_lsn));
	}
#endif /* UNIV_DEBUG */

	log_groups_write_checkpoint_info();

	mutex_exit(&(log_sys->mutex));

	if (sync) {
		/* Wait for the checkpoint write to complete */
		rw_lock_s_lock(&(log_sys->checkpoint_lock));
		rw_lock_s_unlock(&(log_sys->checkpoint_lock));
	}

	return(TRUE);
}

/********************************************************************
Makes a checkpoint at a given lsn or later. */

void
log_make_checkpoint_at(
/*===================*/
	dulint	lsn,		/* in: make a checkpoint at this or a later
				lsn, if ut_dulint_max, makes a checkpoint at
				the latest lsn */
	ibool	write_always)	/* in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
{
	ibool	success;

	/* Preflush pages synchronously */

	success = FALSE;

	while (!success) {
		success = log_preflush_pool_modified_pages(lsn, TRUE);
	}

	success = FALSE;

	while (!success) {
		success = log_checkpoint(TRUE, write_always);
	}
}

/********************************************************************
Tries to establish a big enough margin of free space in the log groups, such
that a new log entry can be catenated without an immediate need for a
checkpoint. NOTE: this function may only be called if the calling thread
owns no synchronization objects! */
static
void
log_checkpoint_margin(void)
/*=======================*/
{
	log_t*	log		= log_sys;
	ulint	age;
	ulint	checkpoint_age;
	ulint	advance;
	dulint	oldest_lsn;
	ibool	sync;
	ibool	checkpoint_sync;
	ibool	do_checkpoint;
	ibool	success;
loop:
	sync = FALSE;
	checkpoint_sync = FALSE;
	do_checkpoint = FALSE;

	mutex_enter(&(log->mutex));

	if (log->check_flush_or_checkpoint == FALSE) {
		mutex_exit(&(log->mutex));

		return;
	}

	oldest_lsn = log_buf_pool_get_oldest_modification();

	age = ut_dulint_minus(log->lsn, oldest_lsn);

	if (age > log->max_modified_age_sync) {

		/* A flush is urgent: we have to do a synchronous preflush */

		sync = TRUE;
		advance = 2 * (age - log->max_modified_age_sync);
	} else if (age > log->max_modified_age_async) {

		/* A flush is not urgent: we do an asynchronous preflush */
		advance = age - log->max_modified_age_async;
	} else {
		advance = 0;
	}

	checkpoint_age = ut_dulint_minus(log->lsn, log->last_checkpoint_lsn);

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
		dulint	new_oldest = ut_dulint_add(oldest_lsn, advance);

		success = log_preflush_pool_modified_pages(new_oldest, sync);

		/* If the flush succeeded, this thread has done its part
		and can proceed. If it did not succeed, there was another
		thread doing a flush at the same time. If sync was FALSE,
		the flush was not urgent, and we let this thread proceed.
		Otherwise, we let it start from the beginning again. */

		if (sync && !success) {
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

/**********************************************************
Reads a specified log segment to a buffer. */

void
log_group_read_log_seg(
/*===================*/
	ulint		type,		/* in: LOG_ARCHIVE or LOG_RECOVER */
	byte*		buf,		/* in: buffer where to read */
	log_group_t*	group,		/* in: log group */
	dulint		start_lsn,	/* in: read area start */
	dulint		end_lsn)	/* in: read area end */
{
	ulint	len;
	ulint	source_offset;
	ibool	sync;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	sync = FALSE;

	if (type == LOG_RECOVER) {
		sync = TRUE;
	}
loop:
	source_offset = log_group_calc_lsn_offset(start_lsn, group);

	len = ut_dulint_minus(end_lsn, start_lsn);

	ut_ad(len != 0);

	if ((source_offset % group->file_size) + len > group->file_size) {

		len = group->file_size - (source_offset % group->file_size);
	}

#ifdef UNIV_LOG_ARCHIVE
	if (type == LOG_ARCHIVE) {

		log_sys->n_pending_archive_ios++;
	}
#endif /* UNIV_LOG_ARCHIVE */

	log_sys->n_log_ios++;

	fil_io(OS_FILE_READ | OS_FILE_LOG, sync, group->space_id,
		source_offset / UNIV_PAGE_SIZE, source_offset % UNIV_PAGE_SIZE,
		len, buf, NULL);

	start_lsn = ut_dulint_add(start_lsn, len);
	buf += len;

	if (ut_dulint_cmp(start_lsn, end_lsn) != 0) {

		goto loop;
	}
}

#ifdef UNIV_LOG_ARCHIVE
/**********************************************************
Generates an archived log file name. */

void
log_archived_file_name_gen(
/*=======================*/
	char*	buf,	/* in: buffer where to write */
	ulint	id __attribute__((unused)),
			/* in: group id;
			currently we only archive the first group */
	ulint	file_no)/* in: file number */
{
	sprintf(buf, "%sib_arch_log_%010lu", srv_arch_dir, (ulong) file_no);
}

/**********************************************************
Writes a log file header to a log file space. */
static
void
log_group_archive_file_header_write(
/*================================*/
	log_group_t*	group,		/* in: log group */
	ulint		nth_file,	/* in: header to the nth file in the
					archive log file space */
	ulint		file_no,	/* in: archived file number */
	dulint		start_lsn)	/* in: log file data starts at this
					lsn */
{
	byte*	buf;
	ulint	dest_offset;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	ut_a(nth_file < group->n_files);

	buf = *(group->archive_file_header_bufs + nth_file);

	mach_write_to_4(buf + LOG_GROUP_ID, group->id);
	mach_write_to_8(buf + LOG_FILE_START_LSN, start_lsn);
	mach_write_to_4(buf + LOG_FILE_NO, file_no);

	mach_write_to_4(buf + LOG_FILE_ARCH_COMPLETED, FALSE);

	dest_offset = nth_file * group->file_size;

	log_sys->n_log_ios++;

	fil_io(OS_FILE_WRITE | OS_FILE_LOG, TRUE, group->archive_space_id,
				dest_offset / UNIV_PAGE_SIZE,
				dest_offset % UNIV_PAGE_SIZE,
				2 * OS_FILE_LOG_BLOCK_SIZE,
				buf, &log_archive_io);
}

/**********************************************************
Writes a log file header to a completed archived log file. */
static
void
log_group_archive_completed_header_write(
/*=====================================*/
	log_group_t*	group,		/* in: log group */
	ulint		nth_file,	/* in: header to the nth file in the
					archive log file space */
	dulint		end_lsn)	/* in: end lsn of the file */
{
	byte*	buf;
	ulint	dest_offset;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(nth_file < group->n_files);

	buf = *(group->archive_file_header_bufs + nth_file);

	mach_write_to_4(buf + LOG_FILE_ARCH_COMPLETED, TRUE);
	mach_write_to_8(buf + LOG_FILE_END_LSN, end_lsn);

	dest_offset = nth_file * group->file_size + LOG_FILE_ARCH_COMPLETED;

	log_sys->n_log_ios++;

	fil_io(OS_FILE_WRITE | OS_FILE_LOG, TRUE, group->archive_space_id,
				dest_offset / UNIV_PAGE_SIZE,
				dest_offset % UNIV_PAGE_SIZE,
				OS_FILE_LOG_BLOCK_SIZE,
				buf + LOG_FILE_ARCH_COMPLETED,
				&log_archive_io);
}

/**********************************************************
Does the archive writes for a single log group. */
static
void
log_group_archive(
/*==============*/
	log_group_t*	group)	/* in: log group */
{
	os_file_t file_handle;
	dulint	start_lsn;
	dulint	end_lsn;
	char	name[1024];
	byte*	buf;
	ulint	len;
	ibool	ret;
	ulint	next_offset;
	ulint	n_files;
	ulint	open_mode;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	start_lsn = log_sys->archived_lsn;

	ut_a(ut_dulint_get_low(start_lsn) % OS_FILE_LOG_BLOCK_SIZE == 0);

	end_lsn = log_sys->next_archived_lsn;

	ut_a(ut_dulint_get_low(end_lsn) % OS_FILE_LOG_BLOCK_SIZE == 0);

	buf = log_sys->archive_buf;

	n_files = 0;

	next_offset = group->archived_offset;
loop:
	if ((next_offset % group->file_size == 0)
		|| (fil_space_get_size(group->archive_space_id) == 0)) {

		/* Add the file to the archive file space; create or open the
		file */

		if (next_offset % group->file_size == 0) {
			open_mode = OS_FILE_CREATE;
		} else {
			open_mode = OS_FILE_OPEN;
		}

		log_archived_file_name_gen(name, group->id,
					group->archived_file_no + n_files);

		file_handle = os_file_create(name, open_mode, OS_FILE_AIO,
						OS_DATA_FILE, &ret);

		if (!ret && (open_mode == OS_FILE_CREATE)) {
			file_handle = os_file_create(name, OS_FILE_OPEN,
					OS_FILE_AIO, OS_DATA_FILE, &ret);
		}

		if (!ret) {
			fprintf(stderr,
		"InnoDB: Cannot create or open archive log file %s.\n"
		"InnoDB: Cannot continue operation.\n"
		"InnoDB: Check that the log archive directory exists,\n"
		"InnoDB: you have access rights to it, and\n"
		"InnoDB: there is space available.\n", name);
			exit(1);
		}

#ifdef UNIV_DEBUG
		if (log_debug_writes) {
			fprintf(stderr, "Created archive file %s\n", name);
		}
#endif /* UNIV_DEBUG */

		ret = os_file_close(file_handle);

		ut_a(ret);

		/* Add the archive file as a node to the space */

		fil_node_create(name, group->file_size / UNIV_PAGE_SIZE,
					group->archive_space_id, FALSE);

		if (next_offset % group->file_size == 0) {
			log_group_archive_file_header_write(group, n_files,
					group->archived_file_no + n_files,
					start_lsn);

			next_offset += LOG_FILE_HDR_SIZE;
		}
	}

	len = ut_dulint_minus(end_lsn, start_lsn);

	if (group->file_size < (next_offset % group->file_size) + len) {

		len = group->file_size - (next_offset % group->file_size);
	}

#ifdef UNIV_DEBUG
	if (log_debug_writes) {
		fprintf(stderr,
		"Archiving starting at lsn %lu %lu, len %lu to group %lu\n",
					(ulong) ut_dulint_get_high(start_lsn),
					(ulong) ut_dulint_get_low(start_lsn),
					(ulong) len, (ulong) group->id);
	}
#endif /* UNIV_DEBUG */

	log_sys->n_pending_archive_ios++;

	log_sys->n_log_ios++;

	fil_io(OS_FILE_WRITE | OS_FILE_LOG, FALSE, group->archive_space_id,
		next_offset / UNIV_PAGE_SIZE, next_offset % UNIV_PAGE_SIZE,
		ut_calc_align(len, OS_FILE_LOG_BLOCK_SIZE), buf,
							&log_archive_io);

	start_lsn = ut_dulint_add(start_lsn, len);
	next_offset += len;
	buf += len;

	if (next_offset % group->file_size == 0) {
		n_files++;
	}

	if (ut_dulint_cmp(end_lsn, start_lsn) != 0) {

		goto loop;
	}

	group->next_archived_file_no = group->archived_file_no + n_files;
	group->next_archived_offset = next_offset % group->file_size;

	ut_a(group->next_archived_offset % OS_FILE_LOG_BLOCK_SIZE == 0);
}

/*********************************************************
(Writes to the archive of each log group.) Currently, only the first
group is archived. */
static
void
log_archive_groups(void)
/*====================*/
{
	log_group_t*	group;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	log_group_archive(group);
}

/*********************************************************
Completes the archiving write phase for (each log group), currently,
the first log group. */
static
void
log_archive_write_complete_groups(void)
/*===================================*/
{
	log_group_t*	group;
	ulint		end_offset;
	ulint		trunc_files;
	ulint		n_files;
	dulint		start_lsn;
	dulint		end_lsn;
	ulint		i;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	group->archived_file_no = group->next_archived_file_no;
	group->archived_offset = group->next_archived_offset;

	/* Truncate from the archive file space all but the last
	file, or if it has been written full, all files */

	n_files = (UNIV_PAGE_SIZE
		* fil_space_get_size(group->archive_space_id))
		/ group->file_size;
	ut_ad(n_files > 0);

	end_offset = group->archived_offset;

	if (end_offset % group->file_size == 0) {

		trunc_files = n_files;
	} else {
		trunc_files = n_files - 1;
	}

#ifdef UNIV_DEBUG
	if (log_debug_writes && trunc_files) {
		fprintf(stderr,
			"Complete file(s) archived to group %lu\n",
							  (ulong) group->id);
	}
#endif /* UNIV_DEBUG */

	/* Calculate the archive file space start lsn */
	start_lsn = ut_dulint_subtract(log_sys->next_archived_lsn,
				end_offset - LOG_FILE_HDR_SIZE
				+ trunc_files
				  * (group->file_size - LOG_FILE_HDR_SIZE));
	end_lsn = start_lsn;

	for (i = 0; i < trunc_files; i++) {

		end_lsn = ut_dulint_add(end_lsn,
					group->file_size - LOG_FILE_HDR_SIZE);

		/* Write a notice to the headers of archived log
		files that the file write has been completed */

		log_group_archive_completed_header_write(group, i, end_lsn);
	}

	fil_space_truncate_start(group->archive_space_id,
					trunc_files * group->file_size);

#ifdef UNIV_DEBUG
	if (log_debug_writes) {
		fputs("Archiving writes completed\n", stderr);
	}
#endif /* UNIV_DEBUG */
}

/**********************************************************
Completes an archiving i/o. */
static
void
log_archive_check_completion_low(void)
/*==================================*/
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (log_sys->n_pending_archive_ios == 0
			&& log_sys->archiving_phase == LOG_ARCHIVE_READ) {

#ifdef UNIV_DEBUG
		if (log_debug_writes) {
			fputs("Archiving read completed\n", stderr);
		}
#endif /* UNIV_DEBUG */

		/* Archive buffer has now been read in: start archive writes */

		log_sys->archiving_phase = LOG_ARCHIVE_WRITE;

		log_archive_groups();
	}

	if (log_sys->n_pending_archive_ios == 0
			&& log_sys->archiving_phase == LOG_ARCHIVE_WRITE) {

		log_archive_write_complete_groups();

		log_sys->archived_lsn = log_sys->next_archived_lsn;

		rw_lock_x_unlock_gen(&(log_sys->archive_lock), LOG_ARCHIVE);
	}
}

/**********************************************************
Completes an archiving i/o. */
static
void
log_io_complete_archive(void)
/*=========================*/
{
	log_group_t*	group;

	mutex_enter(&(log_sys->mutex));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	mutex_exit(&(log_sys->mutex));

	fil_flush(group->archive_space_id);

	mutex_enter(&(log_sys->mutex));

	ut_ad(log_sys->n_pending_archive_ios > 0);

	log_sys->n_pending_archive_ios--;

	log_archive_check_completion_low();

	mutex_exit(&(log_sys->mutex));
}

/************************************************************************
Starts an archiving operation. */

ibool
log_archive_do(
/*===========*/
			/* out: TRUE if succeed, FALSE if an archiving
			operation was already running */
	ibool	sync,	/* in: TRUE if synchronous operation is desired */
	ulint*	n_bytes)/* out: archive log buffer size, 0 if nothing to
			archive */
{
	ibool	calc_new_limit;
	dulint	start_lsn;
	dulint	limit_lsn;

	calc_new_limit = TRUE;
loop:
	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state == LOG_ARCH_OFF) {
		mutex_exit(&(log_sys->mutex));

		*n_bytes = 0;

		return(TRUE);

	} else if (log_sys->archiving_state == LOG_ARCH_STOPPED
		   || log_sys->archiving_state == LOG_ARCH_STOPPING2) {

		mutex_exit(&(log_sys->mutex));

		os_event_wait(log_sys->archiving_on);

		mutex_enter(&(log_sys->mutex));

		goto loop;
	}

	start_lsn = log_sys->archived_lsn;

	if (calc_new_limit) {
		ut_a(log_sys->archive_buf_size % OS_FILE_LOG_BLOCK_SIZE == 0);
		limit_lsn = ut_dulint_add(start_lsn,
						log_sys->archive_buf_size);

		*n_bytes = log_sys->archive_buf_size;

		if (ut_dulint_cmp(limit_lsn, log_sys->lsn) >= 0) {

			limit_lsn = ut_dulint_align_down(log_sys->lsn,
						OS_FILE_LOG_BLOCK_SIZE);
		}
	}

	if (ut_dulint_cmp(log_sys->archived_lsn, limit_lsn) >= 0) {

		mutex_exit(&(log_sys->mutex));

		*n_bytes = 0;

		return(TRUE);
	}

	if (ut_dulint_cmp(log_sys->written_to_all_lsn, limit_lsn) < 0) {

		mutex_exit(&(log_sys->mutex));

		log_write_up_to(limit_lsn, LOG_WAIT_ALL_GROUPS, TRUE);

		calc_new_limit = FALSE;

		goto loop;
	}

	if (log_sys->n_pending_archive_ios > 0) {
		/* An archiving operation is running */

		mutex_exit(&(log_sys->mutex));

		if (sync) {
			rw_lock_s_lock(&(log_sys->archive_lock));
			rw_lock_s_unlock(&(log_sys->archive_lock));
		}

		*n_bytes = log_sys->archive_buf_size;

		return(FALSE);
	}

	rw_lock_x_lock_gen(&(log_sys->archive_lock), LOG_ARCHIVE);

	log_sys->archiving_phase = LOG_ARCHIVE_READ;

	log_sys->next_archived_lsn = limit_lsn;

#ifdef UNIV_DEBUG
	if (log_debug_writes) {
		fprintf(stderr,
			"Archiving from lsn %lu %lu to lsn %lu %lu\n",
			(ulong) ut_dulint_get_high(log_sys->archived_lsn),
			(ulong) ut_dulint_get_low(log_sys->archived_lsn),
			(ulong) ut_dulint_get_high(limit_lsn),
			(ulong) ut_dulint_get_low(limit_lsn));
	}
#endif /* UNIV_DEBUG */

	/* Read the log segment to the archive buffer */

	log_group_read_log_seg(LOG_ARCHIVE, log_sys->archive_buf,
				UT_LIST_GET_FIRST(log_sys->log_groups),
				start_lsn, limit_lsn);

	mutex_exit(&(log_sys->mutex));

	if (sync) {
		rw_lock_s_lock(&(log_sys->archive_lock));
		rw_lock_s_unlock(&(log_sys->archive_lock));
	}

	*n_bytes = log_sys->archive_buf_size;

	return(TRUE);
}

/********************************************************************
Writes the log contents to the archive at least up to the lsn when this
function was called. */
static
void
log_archive_all(void)
/*=================*/
{
	dulint	present_lsn;
	ulint	dummy;

	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state == LOG_ARCH_OFF) {
		mutex_exit(&(log_sys->mutex));

		return;
	}

	present_lsn = log_sys->lsn;

	mutex_exit(&(log_sys->mutex));

	log_pad_current_log_block();

	for (;;) {
		mutex_enter(&(log_sys->mutex));

		if (ut_dulint_cmp(present_lsn, log_sys->archived_lsn) <= 0) {

			mutex_exit(&(log_sys->mutex));

			return;
		}

		mutex_exit(&(log_sys->mutex));

		log_archive_do(TRUE, &dummy);
	}
}

/*********************************************************
Closes the possible open archive log file (for each group) the first group,
and if it was open, increments the group file count by 2, if desired. */
static
void
log_archive_close_groups(
/*=====================*/
	ibool	increment_file_count)	/* in: TRUE if we want to increment
					the file count */
{
	log_group_t*	group;
	ulint		trunc_len;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (log_sys->archiving_state == LOG_ARCH_OFF) {

		return;
	}

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	trunc_len = UNIV_PAGE_SIZE
		* fil_space_get_size(group->archive_space_id);
	if (trunc_len > 0) {
		ut_a(trunc_len == group->file_size);

		/* Write a notice to the headers of archived log
		files that the file write has been completed */

		log_group_archive_completed_header_write(group,
						0, log_sys->archived_lsn);

		fil_space_truncate_start(group->archive_space_id,
								trunc_len);
		if (increment_file_count) {
			group->archived_offset = 0;
			group->archived_file_no += 2;
		}

#ifdef UNIV_DEBUG
		if (log_debug_writes) {
			fprintf(stderr,
			"Incrementing arch file no to %lu in log group %lu\n",
				(ulong) group->archived_file_no + 2,
				(ulong) group->id);
		}
#endif /* UNIV_DEBUG */
	}
}

/********************************************************************
Writes the log contents to the archive up to the lsn when this function was
called, and stops the archiving. When archiving is started again, the archived
log file numbers start from 2 higher, so that the archiving will not write
again to the archived log files which exist when this function returns. */

ulint
log_archive_stop(void)
/*==================*/
			/* out: DB_SUCCESS or DB_ERROR */
{
	ibool	success;

	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state != LOG_ARCH_ON) {

		mutex_exit(&(log_sys->mutex));

		return(DB_ERROR);
	}

	log_sys->archiving_state = LOG_ARCH_STOPPING;

	mutex_exit(&(log_sys->mutex));

	log_archive_all();

	mutex_enter(&(log_sys->mutex));

	log_sys->archiving_state = LOG_ARCH_STOPPING2;
	os_event_reset(log_sys->archiving_on);

	mutex_exit(&(log_sys->mutex));

	/* Wait for a possible archiving operation to end */

	rw_lock_s_lock(&(log_sys->archive_lock));
	rw_lock_s_unlock(&(log_sys->archive_lock));

	mutex_enter(&(log_sys->mutex));

	/* Close all archived log files, incrementing the file count by 2,
	if appropriate */

	log_archive_close_groups(TRUE);

	mutex_exit(&(log_sys->mutex));

	/* Make a checkpoint, so that if recovery is needed, the file numbers
	of new archived log files will start from the right value */

	success = FALSE;

	while (!success) {
		success = log_checkpoint(TRUE, TRUE);
	}

	mutex_enter(&(log_sys->mutex));

	log_sys->archiving_state = LOG_ARCH_STOPPED;

	mutex_exit(&(log_sys->mutex));

	return(DB_SUCCESS);
}

/********************************************************************
Starts again archiving which has been stopped. */

ulint
log_archive_start(void)
/*===================*/
			/* out: DB_SUCCESS or DB_ERROR */
{
	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state != LOG_ARCH_STOPPED) {

		mutex_exit(&(log_sys->mutex));

		return(DB_ERROR);
	}

	log_sys->archiving_state = LOG_ARCH_ON;

	os_event_set(log_sys->archiving_on);

	mutex_exit(&(log_sys->mutex));

	return(DB_SUCCESS);
}

/********************************************************************
Stop archiving the log so that a gap may occur in the archived log files. */

ulint
log_archive_noarchivelog(void)
/*==========================*/
			/* out: DB_SUCCESS or DB_ERROR */
{
loop:
	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state == LOG_ARCH_STOPPED
		|| log_sys->archiving_state == LOG_ARCH_OFF) {

		log_sys->archiving_state = LOG_ARCH_OFF;

		os_event_set(log_sys->archiving_on);

		mutex_exit(&(log_sys->mutex));

		return(DB_SUCCESS);
	}

	mutex_exit(&(log_sys->mutex));

	log_archive_stop();

	os_thread_sleep(500000);

	goto loop;
}

/********************************************************************
Start archiving the log so that a gap may occur in the archived log files. */

ulint
log_archive_archivelog(void)
/*========================*/
			/* out: DB_SUCCESS or DB_ERROR */
{
	mutex_enter(&(log_sys->mutex));

	if (log_sys->archiving_state == LOG_ARCH_OFF) {

		log_sys->archiving_state = LOG_ARCH_ON;

		log_sys->archived_lsn = ut_dulint_align_down(log_sys->lsn,
						OS_FILE_LOG_BLOCK_SIZE);
		mutex_exit(&(log_sys->mutex));

		return(DB_SUCCESS);
	}

	mutex_exit(&(log_sys->mutex));

	return(DB_ERROR);
}

/********************************************************************
Tries to establish a big enough margin of free space in the log groups, such
that a new log entry can be catenated without an immediate need for
archiving. */
static
void
log_archive_margin(void)
/*====================*/
{
	log_t*	log		= log_sys;
	ulint	age;
	ibool	sync;
	ulint	dummy;
loop:
	mutex_enter(&(log->mutex));

	if (log->archiving_state == LOG_ARCH_OFF) {
		mutex_exit(&(log->mutex));

		return;
	}

	age = ut_dulint_minus(log->lsn, log->archived_lsn);

	if (age > log->max_archived_lsn_age) {

		/* An archiving is urgent: we have to do synchronous i/o */

		sync = TRUE;

	} else if (age > log->max_archived_lsn_age_async) {

		/* An archiving is not urgent: we do asynchronous i/o */

		sync = FALSE;
	} else {
		/* No archiving required yet */

		mutex_exit(&(log->mutex));

		return;
	}

	mutex_exit(&(log->mutex));

	log_archive_do(sync, &dummy);

	if (sync == TRUE) {
		/* Check again that enough was written to the archive */

		goto loop;
	}
}
#endif /* UNIV_LOG_ARCHIVE */

/************************************************************************
Checks that there is enough free space in the log to start a new query step.
Flushes the log buffer or makes a new checkpoint if necessary. NOTE: this
function may only be called if the calling thread owns no synchronization
objects! */

void
log_check_margins(void)
/*===================*/
{
loop:
	log_flush_margin();

	log_checkpoint_margin();

#ifdef UNIV_LOG_ARCHIVE
	log_archive_margin();
#endif /* UNIV_LOG_ARCHIVE */

	mutex_enter(&(log_sys->mutex));

	if (log_sys->check_flush_or_checkpoint) {

		mutex_exit(&(log_sys->mutex));

		goto loop;
	}

	mutex_exit(&(log_sys->mutex));
}

/********************************************************************
Makes a checkpoint at the latest lsn and writes it to first page of each
data file in the database, so that we know that the file spaces contain
all modifications up to that lsn. This can only be called at database
shutdown. This function also writes all log in log files to the log archive. */

void
logs_empty_and_mark_files_at_shutdown(void)
/*=======================================*/
{
	dulint	lsn;
	ulint	arch_log_no;

	if (srv_print_verbose_log) {
		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Starting shutdown...\n");
	}
	/* Wait until the master thread and all other operations are idle: our
	algorithm only works if the server is idle at shutdown */

	srv_shutdown_state = SRV_SHUTDOWN_CLEANUP;
loop:
	os_thread_sleep(100000);

	mutex_enter(&kernel_mutex);

	/* Check that there are no longer transactions. We need this wait
	even for the 'very fast' shutdown, because the InnoDB layer may have
	committed or prepared transactions and we don't want to lose
	them. */

	if (trx_n_mysql_transactions > 0
			|| UT_LIST_GET_LEN(trx_sys->trx_list) > 0) {

		mutex_exit(&kernel_mutex);

		goto loop;
	}

	if (srv_fast_shutdown == 2) {
		/* In this fastest shutdown we do not flush the buffer pool:
		it is essentially a 'crash' of the InnoDB server. Make sure
		that the log is all flushed to disk, so that we can recover
		all committed transactions in a crash recovery. We must not
		write the lsn stamps to the data files, since at a startup
		InnoDB deduces from the stamps if the previous shutdown was
		clean. */

		log_buffer_flush_to_disk();

		return; /* We SKIP ALL THE REST !! */
	}

	/* Check that the master thread is suspended */

	if (srv_n_threads_active[SRV_MASTER] != 0) {

		mutex_exit(&kernel_mutex);

		goto loop;
	}

	mutex_exit(&kernel_mutex);

	mutex_enter(&(log_sys->mutex));

	if (
#ifdef UNIV_LOG_ARCHIVE
			log_sys->n_pending_archive_ios ||
#endif /* UNIV_LOG_ARCHIVE */
			log_sys->n_pending_checkpoint_writes ||
			log_sys->n_pending_writes) {

		mutex_exit(&(log_sys->mutex));

		goto loop;
	}

	mutex_exit(&(log_sys->mutex));

	if (!buf_pool_check_no_pending_io()) {

		goto loop;
	}

#ifdef UNIV_LOG_ARCHIVE
	log_archive_all();
#endif /* UNIV_LOG_ARCHIVE */

		log_make_checkpoint_at(ut_dulint_max, TRUE);

	mutex_enter(&(log_sys->mutex));

	lsn = log_sys->lsn;

	if ((ut_dulint_cmp(lsn, log_sys->last_checkpoint_lsn) != 0)
#ifdef UNIV_LOG_ARCHIVE
		|| (srv_log_archive_on
			&& ut_dulint_cmp(lsn,
				ut_dulint_add(log_sys->archived_lsn,
					LOG_BLOCK_HDR_SIZE))
			!= 0)
#endif /* UNIV_LOG_ARCHIVE */
	) {

		mutex_exit(&(log_sys->mutex));

		goto loop;
	}

	arch_log_no = 0;

#ifdef UNIV_LOG_ARCHIVE
	UT_LIST_GET_FIRST(log_sys->log_groups)->archived_file_no;

	if (0 == UT_LIST_GET_FIRST(log_sys->log_groups)->archived_offset) {

		arch_log_no--;
	}

	log_archive_close_groups(TRUE);
#endif /* UNIV_LOG_ARCHIVE */

	mutex_exit(&(log_sys->mutex));

	mutex_enter(&kernel_mutex);
	/* Check that the master thread has stayed suspended */
	if (srv_n_threads_active[SRV_MASTER] != 0) {
		fprintf(stderr,
"InnoDB: Warning: the master thread woke up during shutdown\n");

		mutex_exit(&kernel_mutex);

		goto loop;
	}
	mutex_exit(&kernel_mutex);

	fil_flush_file_spaces(FIL_TABLESPACE);
	fil_flush_file_spaces(FIL_LOG);

	/* The call fil_write_flushed_lsn_to_data_files() will pass the buffer
	pool: therefore it is essential that the buffer pool has been
	completely flushed to disk! (We do not call fil_write... if the
	'very fast' shutdown is enabled.) */

	if (!buf_all_freed()) {

		goto loop;
	}

	/* The lock timeout thread should now have exited */

	if (srv_lock_timeout_and_monitor_active) {

		goto loop;
	}

	/* We now let also the InnoDB error monitor thread to exit */

	srv_shutdown_state = SRV_SHUTDOWN_LAST_PHASE;

	if (srv_error_monitor_active) {

		goto loop;
	}

	/* Make some checks that the server really is quiet */
	ut_a(srv_n_threads_active[SRV_MASTER] == 0);
	ut_a(buf_all_freed());
	ut_a(0 == ut_dulint_cmp(lsn, log_sys->lsn));

	if (ut_dulint_cmp(lsn, srv_start_lsn) < 0) {
		fprintf(stderr,
"InnoDB: Error: log sequence number at shutdown %lu %lu\n"
"InnoDB: is lower than at startup %lu %lu!\n",
			  (ulong) ut_dulint_get_high(lsn),
			  (ulong) ut_dulint_get_low(lsn),
			  (ulong) ut_dulint_get_high(srv_start_lsn),
			  (ulong) ut_dulint_get_low(srv_start_lsn));
	}

	srv_shutdown_lsn = lsn;

		fil_write_flushed_lsn_to_data_files(lsn, arch_log_no);

	fil_flush_file_spaces(FIL_TABLESPACE);

	fil_close_all_files();

	/* Make some checks that the server really is quiet */
	ut_a(srv_n_threads_active[SRV_MASTER] == 0);
	ut_a(buf_all_freed());
	ut_a(0 == ut_dulint_cmp(lsn, log_sys->lsn));
}

/**********************************************************
Checks by parsing that the catenated log segment for a single mtr is
consistent. */

ibool
log_check_log_recs(
/*===============*/
	byte*	buf,		/* in: pointer to the start of the log segment
				in the log_sys->buf log buffer */
	ulint	len,		/* in: segment length in bytes */
	dulint	buf_start_lsn)	/* in: buffer start lsn */
{
	dulint	contiguous_lsn;
	dulint	scanned_lsn;
	byte*	start;
	byte*	end;
	byte*	buf1;
	byte*	scan_buf;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (len == 0) {

		return(TRUE);
	}

	start = ut_align_down(buf, OS_FILE_LOG_BLOCK_SIZE);
	end = ut_align(buf + len, OS_FILE_LOG_BLOCK_SIZE);

	buf1 = mem_alloc((end - start) + OS_FILE_LOG_BLOCK_SIZE);
	scan_buf = ut_align(buf1, OS_FILE_LOG_BLOCK_SIZE);

	ut_memcpy(scan_buf, start, end - start);

	recv_scan_log_recs(TRUE,
				(buf_pool->n_frames -
				recv_n_pool_free_frames) * UNIV_PAGE_SIZE,
				FALSE, scan_buf, end - start,
				ut_dulint_align_down(buf_start_lsn,
						OS_FILE_LOG_BLOCK_SIZE),
			&contiguous_lsn, &scanned_lsn);

	ut_a(ut_dulint_cmp(scanned_lsn, ut_dulint_add(buf_start_lsn, len))
									== 0);
	ut_a(ut_dulint_cmp(recv_sys->recovered_lsn, scanned_lsn) == 0);

	mem_free(buf1);

	return(TRUE);
}

/**********************************************************
Peeks the current lsn. */

ibool
log_peek_lsn(
/*=========*/
			/* out: TRUE if success, FALSE if could not get the
			log system mutex */
	dulint*	lsn)	/* out: if returns TRUE, current lsn is here */
{
	if (0 == mutex_enter_nowait(&(log_sys->mutex), __FILE__, __LINE__)) {
		*lsn = log_sys->lsn;

		mutex_exit(&(log_sys->mutex));

		return(TRUE);
	}

	return(FALSE);
}

/**********************************************************
Prints info of the log. */

void
log_print(
/*======*/
	FILE*	file)	/* in: file where to print */
{
	double	time_elapsed;
	time_t	current_time;

	mutex_enter(&(log_sys->mutex));

	fprintf(file,
		"Log sequence number %lu %lu\n"
		"Log flushed up to   %lu %lu\n"
		"Last checkpoint at  %lu %lu\n",
			(ulong) ut_dulint_get_high(log_sys->lsn),
			(ulong) ut_dulint_get_low(log_sys->lsn),
			(ulong) ut_dulint_get_high(log_sys->flushed_to_disk_lsn),
			(ulong) ut_dulint_get_low(log_sys->flushed_to_disk_lsn),
			(ulong) ut_dulint_get_high(log_sys->last_checkpoint_lsn),
			(ulong) ut_dulint_get_low(log_sys->last_checkpoint_lsn));

	current_time = time(NULL);

	time_elapsed = 0.001 + difftime(current_time,
					log_sys->last_printout_time);
	fprintf(file,
	"%lu pending log writes, %lu pending chkp writes\n"
	"%lu log i/o's done, %.2f log i/o's/second\n",
	(ulong) log_sys->n_pending_writes,
	(ulong) log_sys->n_pending_checkpoint_writes,
	(ulong) log_sys->n_log_ios,
	((log_sys->n_log_ios - log_sys->n_log_ios_old) / time_elapsed));

	log_sys->n_log_ios_old = log_sys->n_log_ios;
	log_sys->last_printout_time = current_time;

	mutex_exit(&(log_sys->mutex));
}

/**************************************************************************
Refreshes the statistics used to print per-second averages. */

void
log_refresh_stats(void)
/*===================*/
{
	log_sys->n_log_ios_old = log_sys->n_log_ios;
	log_sys->last_printout_time = time(NULL);
}
