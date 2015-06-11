/*****************************************************************************

Copyright (c) 1995, 2015, Oracle and/or its affiliates. All rights reserved.
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
@file include/log0log.h
Database log

Created 12/9/1995 Heikki Tuuri
*******************************************************/

#ifndef log0log_h
#define log0log_h

#include "univ.i"
#include "dyn0buf.h"
#ifndef UNIV_HOTBACKUP
#include "sync0rw.h"
#endif /* !UNIV_HOTBACKUP */

/* Type used for all log sequence number storage and arithmetics */
typedef	ib_uint64_t		lsn_t;

#define LSN_MAX			IB_UINT64_MAX

#define LSN_PF			UINT64PF

/** Redo log buffer */
struct log_t;

/** Redo log group */
struct log_group_t;

/** Magic value to use instead of log checksums when they are disabled */
#define LOG_NO_CHECKSUM_MAGIC 0xDEADBEEFUL

typedef ulint (*log_checksum_func_t)(const byte* log_block);

/** Pointer to the log checksum calculation function. Protected with
log_sys->mutex. */
extern log_checksum_func_t log_checksum_algorithm_ptr;

/** Maximum number of log groups in log_group_t::checkpoint_buf */
#define LOG_MAX_N_GROUPS	32

/*******************************************************************//**
Calculates where in log files we find a specified lsn.
@return log file number */
ulint
log_calc_where_lsn_is(
/*==================*/
	int64_t*	log_file_offset,	/*!< out: offset in that file
						(including the header) */
	ib_uint64_t	first_header_lsn,	/*!< in: first log file start
						lsn */
	ib_uint64_t	lsn,			/*!< in: lsn whose position to
						determine */
	ulint		n_log_files,		/*!< in: total number of log
						files */
	int64_t		log_file_size);		/*!< in: log file size
						(including the header) */
#ifndef UNIV_HOTBACKUP
/** Append a string to the log.
@param[in]	str		string
@param[in]	len		string length
@param[out]	start_lsn	start LSN of the log record
@return end lsn of the log record, zero if did not succeed */
UNIV_INLINE
lsn_t
log_reserve_and_write_fast(
	const void*	str,
	ulint		len,
	lsn_t*		start_lsn);
/***********************************************************************//**
Checks if there is need for a log buffer flush or a new checkpoint, and does
this if yes. Any database operation should call this when it has modified
more than about 4 pages. NOTE that this function may only be called when the
OS thread owns no synchronization objects except the dictionary mutex. */
UNIV_INLINE
void
log_free_check(void);
/*================*/

/** Extends the log buffer.
@param[in]	len	requested minimum size in bytes */
void
log_buffer_extend(
	ulint	len);

/** Check margin not to overwrite transaction log from the last checkpoint.
If would estimate the log write to exceed the log_group_capacity,
waits for the checkpoint is done enough.
@param[in]	len	length of the data to be written */

void
log_margin_checkpoint_age(
	ulint	len);

/** Open the log for log_write_low. The log must be closed with log_close.
@param[in]	len	length of the data to be written
@return start lsn of the log record */
lsn_t
log_reserve_and_open(
	ulint	len);
/************************************************************//**
Writes to the log the string given. It is assumed that the caller holds the
log mutex. */
void
log_write_low(
/*==========*/
	const byte*	str,		/*!< in: string */
	ulint		str_len);	/*!< in: string length */
/************************************************************//**
Closes the log.
@return lsn */
lsn_t
log_close(void);
/*===========*/
/************************************************************//**
Gets the current lsn.
@return current lsn */
UNIV_INLINE
lsn_t
log_get_lsn(void);
/*=============*/
/****************************************************************
Gets the log group capacity. It is OK to read the value without
holding log_sys->mutex because it is constant.
@return log group capacity */
UNIV_INLINE
lsn_t
log_get_capacity(void);
/*==================*/
/****************************************************************
Get log_sys::max_modified_age_async. It is OK to read the value without
holding log_sys::mutex because it is constant.
@return max_modified_age_async */
UNIV_INLINE
lsn_t
log_get_max_modified_age_async(void);
/*================================*/
/******************************************************//**
Initializes the log. */
void
log_init(void);
/*==========*/
/******************************************************************//**
Inits a log group to the log system.
@return true if success, false if not */
__attribute__((warn_unused_result))
bool
log_group_init(
/*===========*/
	ulint	id,			/*!< in: group id */
	ulint	n_files,		/*!< in: number of log files */
	lsn_t	file_size,		/*!< in: log file size in bytes */
	ulint	space_id);		/*!< in: space id of the file space
					which contains the log files of this
					group */
/******************************************************//**
Completes an i/o to a log file. */
void
log_io_complete(
/*============*/
	log_group_t*	group);	/*!< in: log group */
/******************************************************//**
This function is called, e.g., when a transaction wants to commit. It checks
that the log has been written to the log file up to the last log entry written
by the transaction. If there is a flush running, it waits and checks if the
flush flushed enough. If not, starts a new flush. */
void
log_write_up_to(
/*============*/
	lsn_t	lsn,	/*!< in: log sequence number up to which
			the log should be written, LSN_MAX if not specified */
	bool	flush_to_disk);
			/*!< in: true if we want the written log
			also to be flushed to disk */
/** write to the log file up to the last log entry.
@param[in]	sync	whether we want the written log
also to be flushed to disk. */
void
log_buffer_flush_to_disk(
	bool sync = true);
/****************************************************************//**
This functions writes the log buffer to the log file and if 'flush'
is set it forces a flush of the log file as well. This is meant to be
called from background master thread only as it does not wait for
the write (+ possible flush) to finish. */
void
log_buffer_sync_in_background(
/*==========================*/
	bool	flush);	/*<! in: flush the logs to disk */
/** Make a checkpoint. Note that this function does not flush dirty
blocks from the buffer pool: it only checks what is lsn of the oldest
modification in the pool, and writes information about the lsn in
log files. Use log_make_checkpoint_at() to flush also the pool.
@param[in]	sync		whether to wait for the write to complete
@param[in]	write_always	force a write even if no log
has been generated since the latest checkpoint
@return true if success, false if a checkpoint write was already running */
bool
log_checkpoint(
	bool	sync,
	bool	write_always);

/** Make a checkpoint at or after a specified LSN.
@param[in]	lsn		the log sequence number, or LSN_MAX
for the latest LSN
@param[in]	write_always	force a write even if no log
has been generated since the latest checkpoint */
void
log_make_checkpoint_at(
	lsn_t			lsn,
	bool			write_always);

/****************************************************************//**
Makes a checkpoint at the latest lsn and writes it to first page of each
data file in the database, so that we know that the file spaces contain
all modifications up to that lsn. This can only be called at database
shutdown. This function also writes all log in log files to the log archive. */
void
logs_empty_and_mark_files_at_shutdown(void);
/*=======================================*/
/******************************************************//**
Reads a checkpoint info from a log group header to log_sys->checkpoint_buf. */
void
log_group_read_checkpoint_info(
/*===========================*/
	log_group_t*	group,	/*!< in: log group */
	ulint		field);	/*!< in: LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2 */
/*******************************************************************//**
Gets info from a checkpoint about a log group. */
void
log_checkpoint_get_nth_group_info(
/*==============================*/
	const byte*	buf,	/*!< in: buffer containing checkpoint info */
	ulint		n,	/*!< in: nth slot */
	ulint*		file_no,/*!< out: archived file number */
	ulint*		offset);/*!< out: archived file offset */
/** Write checkpoint info to the log header and invoke log_mutex_exit().
@param[in]	sync	whether to wait for the write to complete */
void
log_write_checkpoint_info(
	bool	sync);

/** Set extra data to be written to the redo log during checkpoint.
@param[in]	buf	data to be appended on checkpoint, or NULL
@return pointer to previous data to be appended on checkpoint */
mtr_buf_t*
log_append_on_checkpoint(
	mtr_buf_t*	buf);
#else /* !UNIV_HOTBACKUP */
/******************************************************//**
Writes info to a buffer of a log group when log files are created in
backup restoration. */
void
log_reset_first_header_and_checkpoint(
/*==================================*/
	byte*		hdr_buf,/*!< in: buffer which will be written to the
				start of the first log file */
	ib_uint64_t	start);	/*!< in: lsn of the start of the first log file;
				we pretend that there is a checkpoint at
				start + LOG_BLOCK_HDR_SIZE */
#endif /* !UNIV_HOTBACKUP */
/**
Checks that there is enough free space in the log to start a new query step.
Flushes the log buffer or makes a new checkpoint if necessary. NOTE: this
function may only be called if the calling thread owns no synchronization
objects! */
void
log_check_margins(void);
#ifndef UNIV_HOTBACKUP
/******************************************************//**
Reads a specified log segment to a buffer. */
void
log_group_read_log_seg(
/*===================*/
	byte*		buf,		/*!< in: buffer where to read */
	log_group_t*	group,		/*!< in: log group */
	lsn_t		start_lsn,	/*!< in: read area start */
	lsn_t		end_lsn);	/*!< in: read area end */
/********************************************************//**
Sets the field values in group to correspond to a given lsn. For this function
to work, the values must already be correctly initialized to correspond to
some lsn, for instance, a checkpoint lsn. */
void
log_group_set_fields(
/*=================*/
	log_group_t*	group,	/*!< in/out: group */
	lsn_t		lsn);	/*!< in: lsn for which the values should be
				set */
/******************************************************//**
Calculates the data capacity of a log group, when the log file headers are not
included.
@return capacity in bytes */
lsn_t
log_group_get_capacity(
/*===================*/
	const log_group_t*	group);	/*!< in: log group */
#endif /* !UNIV_HOTBACKUP */
/************************************************************//**
Gets a log block flush bit.
@return TRUE if this block was the first to be written in a log flush */
UNIV_INLINE
ibool
log_block_get_flush_bit(
/*====================*/
	const byte*	log_block);	/*!< in: log block */
/************************************************************//**
Gets a log block number stored in the header.
@return log block number stored in the block header */
UNIV_INLINE
ulint
log_block_get_hdr_no(
/*=================*/
	const byte*	log_block);	/*!< in: log block */
/************************************************************//**
Gets a log block data length.
@return log block data length measured as a byte offset from the block start */
UNIV_INLINE
ulint
log_block_get_data_len(
/*===================*/
	const byte*	log_block);	/*!< in: log block */
/************************************************************//**
Sets the log block data length. */
UNIV_INLINE
void
log_block_set_data_len(
/*===================*/
	byte*	log_block,	/*!< in/out: log block */
	ulint	len);		/*!< in: data length */
/************************************************************//**
Calculates the checksum for a log block.
@return checksum */
UNIV_INLINE
ulint
log_block_calc_checksum(
/*====================*/
	const byte*	block);	/*!< in: log block */

/** Calculates the checksum for a log block using the legacy InnoDB algorithm.
@param[in]	block	the redo log block
@return		the calculated checksum value */
UNIV_INLINE
ulint
log_block_calc_checksum_innodb(const byte*	block);

/** Calculates the checksum for a log block using the CRC32 algorithm.
@param[in]	block	log block
@return checksum */
UNIV_INLINE
ulint
log_block_calc_checksum_crc32(
	const byte*	block);

/** Calculates the checksum for a log block using the CRC32 algorithm.
This function uses big endian byteorder when converting byte strings to
integers.
@param[in]	block	log block
@return checksum */
UNIV_INLINE
ulint
log_block_calc_checksum_crc32_legacy_big_endian(
	const byte*	block);

/** Calculates the checksum for a log block using the "no-op" algorithm.
@param[in]	block	the redo log block
@return		the calculated checksum value */
UNIV_INLINE
ulint
log_block_calc_checksum_none(const byte*	block);

/************************************************************//**
Gets a log block checksum field value.
@return checksum */
UNIV_INLINE
ulint
log_block_get_checksum(
/*===================*/
	const byte*	log_block);	/*!< in: log block */
/************************************************************//**
Sets a log block checksum field value. */
UNIV_INLINE
void
log_block_set_checksum(
/*===================*/
	byte*	log_block,	/*!< in/out: log block */
	ulint	checksum);	/*!< in: checksum */
/************************************************************//**
Gets a log block first mtr log record group offset.
@return first mtr log record group byte offset from the block start, 0
if none */
UNIV_INLINE
ulint
log_block_get_first_rec_group(
/*==========================*/
	const byte*	log_block);	/*!< in: log block */
/************************************************************//**
Sets the log block first mtr log record group offset. */
UNIV_INLINE
void
log_block_set_first_rec_group(
/*==========================*/
	byte*	log_block,	/*!< in/out: log block */
	ulint	offset);	/*!< in: offset, 0 if none */
/************************************************************//**
Gets a log block checkpoint number field (4 lowest bytes).
@return checkpoint no (4 lowest bytes) */
UNIV_INLINE
ulint
log_block_get_checkpoint_no(
/*========================*/
	const byte*	log_block);	/*!< in: log block */
/************************************************************//**
Initializes a log block in the log buffer. */
UNIV_INLINE
void
log_block_init(
/*===========*/
	byte*	log_block,	/*!< in: pointer to the log buffer */
	lsn_t	lsn);		/*!< in: lsn within the log block */
#ifdef UNIV_HOTBACKUP
/************************************************************//**
Initializes a log block in the log buffer in the old, < 3.23.52 format, where
there was no checksum yet. */
UNIV_INLINE
void
log_block_init_in_old_format(
/*=========================*/
	byte*	log_block,	/*!< in: pointer to the log buffer */
	lsn_t	lsn);		/*!< in: lsn within the log block */
#endif /* UNIV_HOTBACKUP */
/************************************************************//**
Converts a lsn to a log block number.
@return log block number, it is > 0 and <= 1G */
UNIV_INLINE
ulint
log_block_convert_lsn_to_no(
/*========================*/
	lsn_t	lsn);	/*!< in: lsn of a byte within the block */
/******************************************************//**
Prints info of the log. */
void
log_print(
/*======*/
	FILE*	file);	/*!< in: file where to print */
/******************************************************//**
Peeks the current lsn.
@return TRUE if success, FALSE if could not get the log system mutex */
ibool
log_peek_lsn(
/*=========*/
	lsn_t*	lsn);	/*!< out: if returns TRUE, current lsn is here */
/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
void
log_refresh_stats(void);
/*===================*/
/********************************************************//**
Closes all log groups. */
void
log_group_close_all(void);
/*=====================*/
/********************************************************//**
Shutdown the log system but do not release all the memory. */
void
log_shutdown(void);
/*==============*/
/********************************************************//**
Free the log system data structures. */
void
log_mem_free(void);
/*==============*/

extern log_t*	log_sys;

/* Values used as flags */
#define LOG_FLUSH	7652559
#define LOG_CHECKPOINT	78656949

/* The counting of lsn's starts from this value: this must be non-zero */
#define LOG_START_LSN		((lsn_t) (16 * OS_FILE_LOG_BLOCK_SIZE))

#define LOG_BUFFER_SIZE		(srv_log_buffer_size * UNIV_PAGE_SIZE)

/* Offsets of a log block header */
#define	LOG_BLOCK_HDR_NO	0	/* block number which must be > 0 and
					is allowed to wrap around at 2G; the
					highest bit is set to 1 if this is the
					first log block in a log flush write
					segment */
#define LOG_BLOCK_FLUSH_BIT_MASK 0x80000000UL
					/* mask used to get the highest bit in
					the preceding field */
#define	LOG_BLOCK_HDR_DATA_LEN	4	/* number of bytes of log written to
					this block */
#define	LOG_BLOCK_FIRST_REC_GROUP 6	/* offset of the first start of an
					mtr log record group in this log block,
					0 if none; if the value is the same
					as LOG_BLOCK_HDR_DATA_LEN, it means
					that the first rec group has not yet
					been catenated to this log block, but
					if it will, it will start at this
					offset; an archive recovery can
					start parsing the log records starting
					from this offset in this log block,
					if value not 0 */
#define LOG_BLOCK_CHECKPOINT_NO	8	/* 4 lower bytes of the value of
					log_sys->next_checkpoint_no when the
					log block was last written to: if the
					block has not yet been written full,
					this value is only updated before a
					log buffer flush */
#define LOG_BLOCK_HDR_SIZE	12	/* size of the log block header in
					bytes */

/* Offsets of a log block trailer from the end of the block */
#define	LOG_BLOCK_CHECKSUM	4	/* 4 byte checksum of the log block
					contents; in InnoDB versions
					< 3.23.52 this did not contain the
					checksum but the same value as
					.._HDR_NO */
#define	LOG_BLOCK_TRL_SIZE	4	/* trailer size in bytes */

/* Offsets for a checkpoint field */
#define LOG_CHECKPOINT_NO		0
#define LOG_CHECKPOINT_LSN		8
#define LOG_CHECKPOINT_OFFSET_LOW32	16
#define LOG_CHECKPOINT_LOG_BUF_SIZE	20
#define	LOG_CHECKPOINT_ARCHIVED_LSN	24
#define	LOG_CHECKPOINT_GROUP_ARRAY	32

/* For each value smaller than LOG_MAX_N_GROUPS the following 8 bytes: */

#define LOG_CHECKPOINT_ARCHIVED_FILE_NO	0
#define LOG_CHECKPOINT_ARCHIVED_OFFSET	4

#define	LOG_CHECKPOINT_ARRAY_END	(LOG_CHECKPOINT_GROUP_ARRAY\
							+ LOG_MAX_N_GROUPS * 8)
#define LOG_CHECKPOINT_CHECKSUM_1	LOG_CHECKPOINT_ARRAY_END
#define LOG_CHECKPOINT_CHECKSUM_2	(4 + LOG_CHECKPOINT_ARRAY_END)
#if 0
#define LOG_CHECKPOINT_FSP_FREE_LIMIT	(8 + LOG_CHECKPOINT_ARRAY_END)
					/*!< Not used (0);
					This used to contain the
					current fsp free limit in
					tablespace 0, in units of one
					megabyte.

					This information might have been used
					since mysqlbackup version 0.35 but
					before 1.41 to decide if unused ends of
					non-auto-extending data files
					in space 0 can be truncated.

					This information was made obsolete
					by mysqlbackup --compress. */
#define LOG_CHECKPOINT_FSP_MAGIC_N	(12 + LOG_CHECKPOINT_ARRAY_END)
					/*!< Not used (0);
					This magic number tells if the
					checkpoint contains the above field:
					the field was added to
					InnoDB-3.23.50 and
					removed from MySQL 5.6 */
#define LOG_CHECKPOINT_FSP_MAGIC_N_VAL	1441231243
					/*!< if LOG_CHECKPOINT_FSP_MAGIC_N
					contains this value, then
					LOG_CHECKPOINT_FSP_FREE_LIMIT
					is valid */
#endif
#define LOG_CHECKPOINT_OFFSET_HIGH32	(16 + LOG_CHECKPOINT_ARRAY_END)
#define LOG_CHECKPOINT_SIZE		(20 + LOG_CHECKPOINT_ARRAY_END)


/* Offsets of a log file header */
#define LOG_GROUP_ID		0	/* log group number */
#define LOG_FILE_START_LSN	4	/* lsn of the start of data in this
					log file */
#define LOG_FILE_NO		12	/* 4-byte archived log file number;
					this field is only defined in an
					archived log file */
#define LOG_FILE_WAS_CREATED_BY_HOT_BACKUP 16
					/* a 32-byte field which contains
					the string 'ibbackup' and the
					creation time if the log file was
					created by mysqlbackup --restore;
					when mysqld is first time started
					on the restored database, it can
					print helpful info for the user */
#define	LOG_FILE_ARCH_COMPLETED	OS_FILE_LOG_BLOCK_SIZE
					/* this 4-byte field is TRUE when
					the writing of an archived log file
					has been completed; this field is
					only defined in an archived log file */
#define LOG_FILE_END_LSN	(OS_FILE_LOG_BLOCK_SIZE + 4)
					/* lsn where the archived log file
					at least extends: actually the
					archived log file may extend to a
					later lsn, as long as it is within the
					same log block as this lsn; this field
					is defined only when an archived log
					file has been completely written */
#define LOG_CHECKPOINT_1	OS_FILE_LOG_BLOCK_SIZE
					/* first checkpoint field in the log
					header; we write alternately to the
					checkpoint fields when we make new
					checkpoints; this field is only defined
					in the first log file of a log group */
#define LOG_CHECKPOINT_2	(3 * OS_FILE_LOG_BLOCK_SIZE)
					/* second checkpoint field in the log
					header */
#define LOG_FILE_HDR_SIZE	(4 * OS_FILE_LOG_BLOCK_SIZE)

#define LOG_GROUP_OK		301
#define LOG_GROUP_CORRUPTED	302

typedef ib_mutex_t	LogSysMutex;
typedef ib_mutex_t	FlushOrderMutex;

/** Log group consists of a number of log files, each of the same size; a log
group is implemented as a space in the sense of the module fil0fil. */
struct log_group_t{
	/* The following fields are protected by log_sys->mutex */
	ulint		id;		/*!< log group id */
	ulint		n_files;	/*!< number of files in the group */
	lsn_t		file_size;	/*!< individual log file size in bytes,
					including the log file header */
	ulint		space_id;	/*!< file space which implements the log
					group */
	ulint		state;		/*!< LOG_GROUP_OK or
					LOG_GROUP_CORRUPTED */
	lsn_t		lsn;		/*!< lsn used to fix coordinates within
					the log group */
	lsn_t		lsn_offset;	/*!< the offset of the above lsn */
	byte**		file_header_bufs_ptr;/*!< unaligned buffers */
	byte**		file_header_bufs;/*!< buffers for each file
					header in the group */
	/*-----------------------------*/
	lsn_t		scanned_lsn;	/*!< used only in recovery: recovery scan
					succeeded up to this lsn in this log
					group */
	byte*		checkpoint_buf_ptr;/*!< unaligned checkpoint header */
	byte*		checkpoint_buf;	/*!< checkpoint header is written from
					this buffer to the group */
	UT_LIST_NODE_T(log_group_t)
			log_groups;	/*!< list of log groups */
};

/** Redo log buffer */
struct log_t{
	byte		pad[64];	/*!< padding to prevent other memory
					update hotspots from residing on the
					same memory cache line */
	lsn_t		lsn;		/*!< log sequence number */
	ulint		buf_free;	/*!< first free offset within the log
					buffer */
#ifndef UNIV_HOTBACKUP
	LogSysMutex	mutex;		/*!< mutex protecting the log */

	FlushOrderMutex	log_flush_order_mutex;/*!< mutex to serialize access to
					the flush list when we are putting
					dirty blocks in the list. The idea
					behind this mutex is to be able
					to release log_sys->mutex during
					mtr_commit and still ensure that
					insertions in the flush_list happen
					in the LSN order. */
#endif /* !UNIV_HOTBACKUP */
	byte*		buf_ptr;	/* unaligned log buffer */
	byte*		buf;		/*!< log buffer */
	ulint		buf_size;	/*!< log buffer size in bytes */
	ulint		max_buf_free;	/*!< recommended maximum value of
					buf_free, after which the buffer is
					flushed */
	bool		check_flush_or_checkpoint;
					/*!< this is set when there may
					be need to flush the log buffer, or
					preflush buffer pool pages, or make
					a checkpoint; this MUST be TRUE when
					lsn - last_checkpoint_lsn >
					max_checkpoint_age; this flag is
					peeked at by log_free_check(), which
					does not reserve the log mutex */
	UT_LIST_BASE_NODE_T(log_group_t)
			log_groups;	/*!< log groups */

#ifndef UNIV_HOTBACKUP
	/** The fields involved in the log buffer flush @{ */

	ulint		buf_next_to_write;/*!< first offset in the log buffer
					where the byte content may not exist
					written to file, e.g., the start
					offset of a log record catenated
					later; this is advanced when a flush
					operation is completed to all the log
					groups */
	volatile bool	is_extending;	/*!< this is set to true during extend
					the log buffer size */
	lsn_t		write_lsn;	/*!< last written lsn */
	ulint		write_end_offset;/*!< the data in buffer has
					been written up to this offset
					when the current write ends:
					this field will then be copied
					to buf_next_to_write */
	lsn_t		current_flush_lsn;/*!< end lsn for the current running
					write + flush operation */
	lsn_t		flushed_to_disk_lsn;
					/*!< how far we have written the log
					AND flushed to disk */
	ulint		n_pending_flushes;/*!< number of currently
					pending flushes; incrementing is
					protected by the log mutex;
					may be decremented between
					resetting and setting flush_event */
	os_event_t	flush_event;	/*!< this event is in the reset state
					when a flush is running; a thread
					should wait for this without
					owning the log mutex, but NOTE that
					to set this event, the
					thread MUST own the log mutex! */
	ulint		n_log_ios;	/*!< number of log i/os initiated thus
					far */
	ulint		n_log_ios_old;	/*!< number of log i/o's at the
					previous printout */
	time_t		last_printout_time;/*!< when log_print was last time
					called */
	/* @} */

	/** Fields involved in checkpoints @{ */
	lsn_t		log_group_capacity; /*!< capacity of the log group; if
					the checkpoint age exceeds this, it is
					a serious error because it is possible
					we will then overwrite log and spoil
					crash recovery */
	lsn_t		max_modified_age_async;
					/*!< when this recommended
					value for lsn -
					buf_pool_get_oldest_modification()
					is exceeded, we start an
					asynchronous preflush of pool pages */
	lsn_t		max_modified_age_sync;
					/*!< when this recommended
					value for lsn -
					buf_pool_get_oldest_modification()
					is exceeded, we start a
					synchronous preflush of pool pages */
	lsn_t		max_checkpoint_age_async;
					/*!< when this checkpoint age
					is exceeded we start an
					asynchronous writing of a new
					checkpoint */
	lsn_t		max_checkpoint_age;
					/*!< this is the maximum allowed value
					for lsn - last_checkpoint_lsn when a
					new query step is started */
	ib_uint64_t	next_checkpoint_no;
					/*!< next checkpoint number */
	lsn_t		last_checkpoint_lsn;
					/*!< latest checkpoint lsn */
	lsn_t		next_checkpoint_lsn;
					/*!< next checkpoint lsn */
	mtr_buf_t*	append_on_checkpoint;
					/*!< extra redo log records to write
					during a checkpoint, or NULL if none.
					The pointer is protected by
					log_sys->mutex, and the data must
					remain constant as long as this
					pointer is not NULL. */
	ulint		n_pending_checkpoint_writes;
					/*!< number of currently pending
					checkpoint writes */
	rw_lock_t	checkpoint_lock;/*!< this latch is x-locked when a
					checkpoint write is running; a thread
					should wait for this without owning
					the log mutex */
#endif /* !UNIV_HOTBACKUP */
	byte*		checkpoint_buf_ptr;/* unaligned checkpoint header */
	byte*		checkpoint_buf;	/*!< checkpoint header is read to this
					buffer */
	/* @} */
};

/** Test if flush order mutex is owned. */
#define log_flush_order_mutex_own()			\
	mutex_own(&log_sys->log_flush_order_mutex)

/** Acquire the flush order mutex. */
#define log_flush_order_mutex_enter() do {		\
	mutex_enter(&log_sys->log_flush_order_mutex);	\
} while (0)
/** Release the flush order mutex. */
# define log_flush_order_mutex_exit() do {		\
	mutex_exit(&log_sys->log_flush_order_mutex);	\
} while (0)

/** Test if log sys mutex is owned. */
#define log_mutex_own() mutex_own(&log_sys->mutex)

/** Acquire the log sys mutex. */
#define log_mutex_enter() mutex_enter(&log_sys->mutex)

/** Release the log sys mutex. */
#define log_mutex_exit() mutex_exit(&log_sys->mutex)

#ifndef UNIV_NONINL
#include "log0log.ic"
#endif

#endif
