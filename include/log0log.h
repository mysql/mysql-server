/*****************************************************************************

Copyright (c) 1995, 2010, Oracle and/or its affiliates. All Rights Reserved.
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
#include "ut0byte.h"
#include "ut0lst.h"
#ifndef UNIV_HOTBACKUP
#include "sync0sync.h"
#include "sync0rw.h"
#endif /* !UNIV_HOTBACKUP */

/* Type used for all log sequence number storage and arithmetics */
typedef	ib_uint64_t		lsn_t;

/** Redo log buffer */
typedef struct log_struct	log_t;
/** Redo log group */
typedef struct log_group_struct	log_group_t;

#ifdef UNIV_DEBUG
/** Flag: write to log file? */
extern	ibool	log_do_write;
/** Flag: enable debug output when writing to the log? */
extern	ibool	log_debug_writes;
#else /* UNIV_DEBUG */
/** Write to log */
# define log_do_write TRUE
#endif /* UNIV_DEBUG */

/** Wait modes for log_write_up_to @{ */
#define LOG_NO_WAIT		91
#define LOG_WAIT_ONE_GROUP	92
#define	LOG_WAIT_ALL_GROUPS	93
/* @} */
/** Maximum number of log groups in log_group_struct::checkpoint_buf */
#define LOG_MAX_N_GROUPS	32

#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Sets the global variable log_fsp_current_free_limit. Also makes a checkpoint,
so that we know that the limit has been written to a log checkpoint field
on disk. */
UNIV_INTERN
void
log_fsp_current_free_limit_set_and_checkpoint(
/*==========================================*/
	ulint	limit);	/*!< in: limit to set */
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
	ib_int64_t	log_file_size);		/*!< in: log file size
						(including the header) */
#ifndef UNIV_HOTBACKUP
/************************************************************//**
Writes to the log the string given. The log must be released with
log_release.
@return	end lsn of the log record, zero if did not succeed */
UNIV_INLINE
ib_uint64_t
log_reserve_and_write_fast(
/*=======================*/
	const void*	str,	/*!< in: string */
	ulint		len,	/*!< in: string length */
	ib_uint64_t*	start_lsn);/*!< out: start lsn of the log record */
/***********************************************************************//**
Releases the log mutex. */
UNIV_INLINE
void
log_release(void);
/*=============*/
/***********************************************************************//**
Checks if there is need for a log buffer flush or a new checkpoint, and does
this if yes. Any database operation should call this when it has modified
more than about 4 pages. NOTE that this function may only be called when the
OS thread owns no synchronization objects except the dictionary mutex. */
UNIV_INLINE
void
log_free_check(void);
/*================*/
/**************************************************************************//**
Locks the log mutex and opens the log for log_write_low. The log must be closed
with log_close and released with log_release.
@return start lsn of the log record */
UNIV_INLINE
ib_uint64_t
log_reserve_and_open(
/*=================*/
	ulint	len);	/*!< in: length of data to be catenated */
/************************************************************//**
Opens the log for log_write_low. The log must be closed with log_close.
@return	start lsn of the log record */
UNIV_INTERN
ib_uint64_t
log_open(
/*=====*/
	ulint	len);	/*!< in: length of data to be catenated */
/************************************************************//**
Writes to the log the string given. It is assumed that the caller holds the
log mutex. */
UNIV_INTERN
void
log_write_low(
/*==========*/
	byte*	str,		/*!< in: string */
	ulint	str_len);	/*!< in: string length */
/************************************************************//**
Closes the log.
@return	lsn */
UNIV_INTERN
ib_uint64_t
log_close(void);
/*===========*/
/************************************************************//**
Gets the current lsn.
@return	current lsn */
UNIV_INLINE
ib_uint64_t
log_get_lsn(void);
/*=============*/
/****************************************************************
Gets the log group capacity. It is OK to read the value without
holding log_sys->mutex because it is constant.
@return	log group capacity */
UNIV_INLINE
ulint
log_get_capacity(void);
/*==================*/
/******************************************************//**
Initializes the log. */
UNIV_INTERN
void
log_init(void);
/*==========*/
/******************************************************************//**
Inits a log group to the log system. */
UNIV_INTERN
void
log_group_init(
/*===========*/
	ulint	id,			/*!< in: group id */
	ulint	n_files,		/*!< in: number of log files */
	ulint	file_size,		/*!< in: log file size in bytes */
	ulint	space_id,		/*!< in: space id of the file space
					which contains the log files of this
					group */
	ulint	archive_space_id);	/*!< in: space id of the file space
					which contains some archived log
					files for this group; currently, only
					for the first log group this is
					used */
/******************************************************//**
Completes an i/o to a log file. */
UNIV_INTERN
void
log_io_complete(
/*============*/
	log_group_t*	group);	/*!< in: log group */
/******************************************************//**
This function is called, e.g., when a transaction wants to commit. It checks
that the log has been written to the log file up to the last log entry written
by the transaction. If there is a flush running, it waits and checks if the
flush flushed enough. If not, starts a new flush. */
UNIV_INTERN
void
log_write_up_to(
/*============*/
	ib_uint64_t	lsn,	/*!< in: log sequence number up to which
				the log should be written,
				IB_ULONGLONG_MAX if not specified */
	ulint		wait,	/*!< in: LOG_NO_WAIT, LOG_WAIT_ONE_GROUP,
				or LOG_WAIT_ALL_GROUPS */
	ibool		flush_to_disk);
				/*!< in: TRUE if we want the written log
				also to be flushed to disk */
/****************************************************************//**
Does a syncronous flush of the log buffer to disk. */
UNIV_INTERN
void
log_buffer_flush_to_disk(void);
/*==========================*/
/****************************************************************//**
This functions writes the log buffer to the log file and if 'flush'
is set it forces a flush of the log file as well. This is meant to be
called from background master thread only as it does not wait for
the write (+ possible flush) to finish. */
UNIV_INTERN
void
log_buffer_sync_in_background(
/*==========================*/
	ibool	flush);	/*<! in: flush the logs to disk */
/****************************************************************//**
Advances the smallest lsn for which there are unflushed dirty blocks in the
buffer pool and also may make a new checkpoint. NOTE: this function may only
be called if the calling thread owns no synchronization objects!
@return FALSE if there was a flush batch of the same type running,
which means that we could not start this flush batch */
UNIV_INTERN
ibool
log_preflush_pool_modified_pages(
/*=============================*/
	ib_uint64_t	new_oldest,	/*!< in: try to advance
					oldest_modified_lsn at least
					to this lsn */
	ibool		sync);		/*!< in: TRUE if synchronous
					operation is desired */
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
	ibool	write_always);	/*!< in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
/****************************************************************//**
Makes a checkpoint at a given lsn or later. */
UNIV_INTERN
void
log_make_checkpoint_at(
/*===================*/
	ib_uint64_t	lsn,		/*!< in: make a checkpoint at this or a
					later lsn, if IB_ULONGLONG_MAX, makes
					a checkpoint at the latest lsn */
	ibool		write_always);	/*!< in: the function normally checks if
					the new checkpoint would have a
					greater lsn than the previous one: if
					not, then no physical write is done;
					by setting this parameter TRUE, a
					physical write will always be made to
					log files */
/****************************************************************//**
Makes a checkpoint at the latest lsn and writes it to first page of each
data file in the database, so that we know that the file spaces contain
all modifications up to that lsn. This can only be called at database
shutdown. This function also writes all log in log files to the log archive. */
UNIV_INTERN
void
logs_empty_and_mark_files_at_shutdown(void);
/*=======================================*/
/******************************************************//**
Reads a checkpoint info from a log group header to log_sys->checkpoint_buf. */
UNIV_INTERN
void
log_group_read_checkpoint_info(
/*===========================*/
	log_group_t*	group,	/*!< in: log group */
	ulint		field);	/*!< in: LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2 */
/*******************************************************************//**
Gets info from a checkpoint about a log group. */
UNIV_INTERN
void
log_checkpoint_get_nth_group_info(
/*==============================*/
	const byte*	buf,	/*!< in: buffer containing checkpoint info */
	ulint		n,	/*!< in: nth slot */
	ulint*		file_no,/*!< out: archived file number */
	ulint*		offset);/*!< out: archived file offset */
/******************************************************//**
Writes checkpoint info to groups. */
UNIV_INTERN
void
log_groups_write_checkpoint_info(void);
/*==================================*/
/********************************************************************//**
Starts an archiving operation.
@return	TRUE if succeed, FALSE if an archiving operation was already running */
UNIV_INTERN
ibool
log_archive_do(
/*===========*/
	ibool	sync,	/*!< in: TRUE if synchronous operation is desired */
	ulint*	n_bytes);/*!< out: archive log buffer size, 0 if nothing to
			archive */
/****************************************************************//**
Writes the log contents to the archive up to the lsn when this function was
called, and stops the archiving. When archiving is started again, the archived
log file numbers start from a number one higher, so that the archiving will
not write again to the archived log files which exist when this function
returns.
@return	DB_SUCCESS or DB_ERROR */
UNIV_INTERN
ulint
log_archive_stop(void);
/*==================*/
/****************************************************************//**
Starts again archiving which has been stopped.
@return	DB_SUCCESS or DB_ERROR */
UNIV_INTERN
ulint
log_archive_start(void);
/*===================*/
/****************************************************************//**
Stop archiving the log so that a gap may occur in the archived log files.
@return	DB_SUCCESS or DB_ERROR */
UNIV_INTERN
ulint
log_archive_noarchivelog(void);
/*==========================*/
/****************************************************************//**
Start archiving the log so that a gap may occur in the archived log files.
@return	DB_SUCCESS or DB_ERROR */
UNIV_INTERN
ulint
log_archive_archivelog(void);
/*========================*/
/******************************************************//**
Generates an archived log file name. */
UNIV_INTERN
void
log_archived_file_name_gen(
/*=======================*/
	char*	buf,	/*!< in: buffer where to write */
	ulint	id,	/*!< in: group id */
	ulint	file_no);/*!< in: file number */
#else /* !UNIV_HOTBACKUP */
/******************************************************//**
Writes info to a buffer of a log group when log files are created in
backup restoration. */
UNIV_INTERN
void
log_reset_first_header_and_checkpoint(
/*==================================*/
	byte*		hdr_buf,/*!< in: buffer which will be written to the
				start of the first log file */
	ib_uint64_t	start);	/*!< in: lsn of the start of the first log file;
				we pretend that there is a checkpoint at
				start + LOG_BLOCK_HDR_SIZE */
#endif /* !UNIV_HOTBACKUP */
/********************************************************************//**
Checks that there is enough free space in the log to start a new query step.
Flushes the log buffer or makes a new checkpoint if necessary. NOTE: this
function may only be called if the calling thread owns no synchronization
objects! */
UNIV_INTERN
void
log_check_margins(void);
/*===================*/
#ifndef UNIV_HOTBACKUP
/******************************************************//**
Reads a specified log segment to a buffer. */
UNIV_INTERN
void
log_group_read_log_seg(
/*===================*/
	ulint		type,		/*!< in: LOG_ARCHIVE or LOG_RECOVER */
	byte*		buf,		/*!< in: buffer where to read */
	log_group_t*	group,		/*!< in: log group */
	ib_uint64_t	start_lsn,	/*!< in: read area start */
	ib_uint64_t	end_lsn,	/*!< in: read area end */
	ibool		release_mutex);	/*!< in: whether the log_sys->mutex
				        should be released before the read */
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
	ib_uint64_t	start_lsn,	/*!< in: start lsn of the buffer; must
					be divisible by
					OS_FILE_LOG_BLOCK_SIZE */
	ulint		new_data_offset);/*!< in: start offset of new data in
					buf: this parameter is used to decide
					if we have to write a new log file
					header */
/********************************************************//**
Sets the field values in group to correspond to a given lsn. For this function
to work, the values must already be correctly initialized to correspond to
some lsn, for instance, a checkpoint lsn. */
UNIV_INTERN
void
log_group_set_fields(
/*=================*/
	log_group_t*	group,	/*!< in/out: group */
	ib_uint64_t	lsn);	/*!< in: lsn for which the values should be
				set */
/******************************************************//**
Calculates the data capacity of a log group, when the log file headers are not
included.
@return	capacity in bytes */
UNIV_INTERN
ulint
log_group_get_capacity(
/*===================*/
	const log_group_t*	group);	/*!< in: log group */
#endif /* !UNIV_HOTBACKUP */
/************************************************************//**
Gets a log block flush bit.
@return	TRUE if this block was the first to be written in a log flush */
UNIV_INLINE
ibool
log_block_get_flush_bit(
/*====================*/
	const byte*	log_block);	/*!< in: log block */
/************************************************************//**
Gets a log block number stored in the header.
@return	log block number stored in the block header */
UNIV_INLINE
ulint
log_block_get_hdr_no(
/*=================*/
	const byte*	log_block);	/*!< in: log block */
/************************************************************//**
Gets a log block data length.
@return	log block data length measured as a byte offset from the block start */
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
@return	checksum */
UNIV_INLINE
ulint
log_block_calc_checksum(
/*====================*/
	const byte*	block);	/*!< in: log block */
/************************************************************//**
Gets a log block checksum field value.
@return	checksum */
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
@return	checkpoint no (4 lowest bytes) */
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
	byte*		log_block,	/*!< in: pointer to the log buffer */
	ib_uint64_t	lsn);		/*!< in: lsn within the log block */
/************************************************************//**
Initializes a log block in the log buffer in the old, < 3.23.52 format, where
there was no checksum yet. */
UNIV_INLINE
void
log_block_init_in_old_format(
/*=========================*/
	byte*		log_block,	/*!< in: pointer to the log buffer */
	ib_uint64_t	lsn);		/*!< in: lsn within the log block */
/************************************************************//**
Converts a lsn to a log block number.
@return	log block number, it is > 0 and <= 1G */
UNIV_INLINE
ulint
log_block_convert_lsn_to_no(
/*========================*/
	ib_uint64_t	lsn);	/*!< in: lsn of a byte within the block */
/******************************************************//**
Prints info of the log. */
UNIV_INTERN
void
log_print(
/*======*/
	FILE*	file);	/*!< in: file where to print */
/******************************************************//**
Peeks the current lsn.
@return	TRUE if success, FALSE if could not get the log system mutex */
UNIV_INTERN
ibool
log_peek_lsn(
/*=========*/
	ib_uint64_t*	lsn);	/*!< out: if returns TRUE, current lsn is here */
/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
UNIV_INTERN
void
log_refresh_stats(void);
/*===================*/
/**********************************************************
Shutdown the log system but do not release all the memory. */
UNIV_INTERN
void
log_shutdown(void);
/*==============*/
/**********************************************************
Free the log system data structures. */
UNIV_INTERN
void
log_mem_free(void);
/*==============*/

extern log_t*	log_sys;

/* Values used as flags */
#define LOG_FLUSH	7652559
#define LOG_CHECKPOINT	78656949
#ifdef UNIV_LOG_ARCHIVE
# define LOG_ARCHIVE	11122331
#endif /* UNIV_LOG_ARCHIVE */
#define LOG_RECOVER	98887331

/* The counting of lsn's starts from this value: this must be non-zero */
#define LOG_START_LSN		((ib_uint64_t) (16 * OS_FILE_LOG_BLOCK_SIZE))

#define LOG_BUFFER_SIZE		(srv_log_buffer_size * UNIV_PAGE_SIZE)
#define LOG_ARCHIVE_BUF_SIZE	(srv_log_buffer_size * UNIV_PAGE_SIZE / 4)

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
#define LOG_CHECKPOINT_OFFSET		16
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
#define LOG_CHECKPOINT_FSP_FREE_LIMIT	(8 + LOG_CHECKPOINT_ARRAY_END)
					/* current fsp free limit in
					tablespace 0, in units of one
					megabyte; this information is only used
					by ibbackup to decide if it can
					truncate unused ends of
					non-auto-extending data files in space
					0 */
#define LOG_CHECKPOINT_FSP_MAGIC_N	(12 + LOG_CHECKPOINT_ARRAY_END)
					/* this magic number tells if the
					checkpoint contains the above field:
					the field was added to
					InnoDB-3.23.50 */
#define LOG_CHECKPOINT_SIZE		(16 + LOG_CHECKPOINT_ARRAY_END)

#define LOG_CHECKPOINT_FSP_MAGIC_N_VAL	1441231243

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
					created by ibbackup --restore;
					when mysqld is first time started
					on the restored database, it can
					print helpful info for the user */
#define LOG_FILE_OS_FILE_LOG_BLOCK_SIZE 64
					/* extend to record log_block_size
					of XtraDB. 0 means default 512 */
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

/** Log group consists of a number of log files, each of the same size; a log
group is implemented as a space in the sense of the module fil0fil. */
struct log_group_struct{
	/* The following fields are protected by log_sys->mutex */
	ulint		id;		/*!< log group id */
	ulint		n_files;	/*!< number of files in the group */
	ulint		file_size;	/*!< individual log file size in bytes,
					including the log file header */
	ulint		space_id;	/*!< file space which implements the log
					group */
	ulint		state;		/*!< LOG_GROUP_OK or
					LOG_GROUP_CORRUPTED */
	ib_uint64_t	lsn;		/*!< lsn used to fix coordinates within
					the log group */
	ulint		lsn_offset;	/*!< the offset of the above lsn */
	ulint		n_pending_writes;/*!< number of currently pending flush
					writes for this log group */
	byte**		file_header_bufs_ptr;/*!< unaligned buffers */
	byte**		file_header_bufs;/*!< buffers for each file
					header in the group */
#ifdef UNIV_LOG_ARCHIVE
	/*-----------------------------*/
	byte**		archive_file_header_bufs_ptr;/*!< unaligned buffers */
	byte**		archive_file_header_bufs;/*!< buffers for each file
					header in the group */
	ulint		archive_space_id;/*!< file space which
					implements the log group
					archive */
	ulint		archived_file_no;/*!< file number corresponding to
					log_sys->archived_lsn */
	ulint		archived_offset;/*!< file offset corresponding to
					log_sys->archived_lsn, 0 if we have
					not yet written to the archive file
					number archived_file_no */
	ulint		next_archived_file_no;/*!< during an archive write,
					until the write is completed, we
					store the next value for
					archived_file_no here: the write
					completion function then sets the new
					value to ..._file_no */
	ulint		next_archived_offset; /*!< like the preceding field */
#endif /* UNIV_LOG_ARCHIVE */
	/*-----------------------------*/
	ib_uint64_t	scanned_lsn;	/*!< used only in recovery: recovery scan
					succeeded up to this lsn in this log
					group */
	byte*		checkpoint_buf_ptr;/*!< unaligned checkpoint header */
	byte*		checkpoint_buf;	/*!< checkpoint header is written from
					this buffer to the group */
	UT_LIST_NODE_T(log_group_t)
			log_groups;	/*!< list of log groups */
};

/** Redo log buffer */
struct log_struct{
	byte		pad[64];	/*!< padding to prevent other memory
					update hotspots from residing on the
					same memory cache line */
	ib_uint64_t	lsn;		/*!< log sequence number */
	ulint		buf_free;	/*!< first free offset within the log
					buffer */
#ifndef UNIV_HOTBACKUP
	mutex_t		mutex;		/*!< mutex protecting the log */

	mutex_t		log_flush_order_mutex;/*!< mutex to serialize access to
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
	ulint		old_buf_free;	/*!< value of buf free when log was
					last time opened; only in the debug
					version */
	ib_uint64_t	old_lsn;	/*!< value of lsn when log was
					last time opened; only in the
					debug version */
	ibool		check_flush_or_checkpoint;
					/*!< this is set to TRUE when there may
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
	volatile ibool	is_extending;	/*!< this is set to true during extend
					the log buffer size */
	ib_uint64_t	written_to_some_lsn;
					/*!< first log sequence number not yet
					written to any log group; for this to
					be advanced, it is enough that the
					write i/o has been completed for any
					one log group */
	ib_uint64_t	written_to_all_lsn;
					/*!< first log sequence number not yet
					written to some log group; for this to
					be advanced, it is enough that the
					write i/o has been completed for all
					log groups.
					Note that since InnoDB currently
					has only one log group therefore
					this value is redundant. Also it
					is possible that this value
					falls behind the
					flushed_to_disk_lsn transiently.
					It is appropriate to use either
					flushed_to_disk_lsn or
					write_lsn which are always
					up-to-date and accurate. */
	ib_uint64_t	write_lsn;	/*!< end lsn for the current running
					write */
	ulint		write_end_offset;/*!< the data in buffer has
					been written up to this offset
					when the current write ends:
					this field will then be copied
					to buf_next_to_write */
	ib_uint64_t	current_flush_lsn;/*!< end lsn for the current running
					write + flush operation */
	ib_uint64_t	flushed_to_disk_lsn;
					/*!< how far we have written the log
					AND flushed to disk */
	ulint		n_pending_writes;/*!< number of currently
					pending flushes or writes */
	/* NOTE on the 'flush' in names of the fields below: starting from
	4.0.14, we separate the write of the log file and the actual fsync()
	or other method to flush it to disk. The names below shhould really
	be 'flush_or_write'! */
	os_event_t	no_flush_event;	/*!< this event is in the reset state
					when a flush or a write is running;
					a thread should wait for this without
					owning the log mutex, but NOTE that
					to set or reset this event, the
					thread MUST own the log mutex! */
	ibool		one_flushed;	/*!< during a flush, this is
					first FALSE and becomes TRUE
					when one log group has been
					written or flushed */
	os_event_t	one_flushed_event;/*!< this event is reset when the
					flush or write has not yet completed
					for any log group; e.g., this means
					that a transaction has been committed
					when this is set; a thread should wait
					for this without owning the log mutex,
					but NOTE that to set or reset this
					event, the thread MUST own the log
					mutex! */
	ulint		n_log_ios;	/*!< number of log i/os initiated thus
					far */
	ulint		n_log_ios_old;	/*!< number of log i/o's at the
					previous printout */
	time_t		last_printout_time;/*!< when log_print was last time
					called */
	/* @} */

	/** Fields involved in checkpoints @{ */
	ulint		log_group_capacity; /*!< capacity of the log group; if
					the checkpoint age exceeds this, it is
					a serious error because it is possible
					we will then overwrite log and spoil
					crash recovery */
	ulint		max_modified_age_async;
					/*!< when this recommended
					value for lsn -
					buf_pool_get_oldest_modification()
					is exceeded, we start an
					asynchronous preflush of pool pages */
	ulint		max_modified_age_sync;
					/*!< when this recommended
					value for lsn -
					buf_pool_get_oldest_modification()
					is exceeded, we start a
					synchronous preflush of pool pages */
	ulint		adm_checkpoint_interval;
					/*!< administrator-specified checkpoint
					interval in terms of log growth in
					bytes; the interval actually used by
					the database can be smaller */
	ulint		max_checkpoint_age_async;
					/*!< when this checkpoint age
					is exceeded we start an
					asynchronous writing of a new
					checkpoint */
	ulint		max_checkpoint_age;
					/*!< this is the maximum allowed value
					for lsn - last_checkpoint_lsn when a
					new query step is started */
	ib_uint64_t	next_checkpoint_no;
					/*!< next checkpoint number */
	ib_uint64_t	last_checkpoint_lsn;
					/*!< latest checkpoint lsn */
	ib_uint64_t	next_checkpoint_lsn;
					/*!< next checkpoint lsn */
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
#ifdef UNIV_LOG_ARCHIVE
	/** Fields involved in archiving @{ */
	ulint		archiving_state;/*!< LOG_ARCH_ON, LOG_ARCH_STOPPING
					LOG_ARCH_STOPPED, LOG_ARCH_OFF */
	ib_uint64_t	archived_lsn;	/*!< archiving has advanced to this
					lsn */
	ulint		max_archived_lsn_age_async;
					/*!< recommended maximum age of
					archived_lsn, before we start
					asynchronous copying to the archive */
	ulint		max_archived_lsn_age;
					/*!< maximum allowed age for
					archived_lsn */
	ib_uint64_t	next_archived_lsn;/*!< during an archive write,
					until the write is completed, we
					store the next value for
					archived_lsn here: the write
					completion function then sets the new
					value to archived_lsn */
	ulint		archiving_phase;/*!< LOG_ARCHIVE_READ or
					LOG_ARCHIVE_WRITE */
	ulint		n_pending_archive_ios;
					/*!< number of currently pending reads
					or writes in archiving */
	rw_lock_t	archive_lock;	/*!< this latch is x-locked when an
					archive write is running; a thread
					should wait for this without owning
					the log mutex */
	ulint		archive_buf_size;/*!< size of archive_buf */
	byte*		archive_buf;	/*!< log segment is written to the
					archive from this buffer */
	os_event_t	archiving_on;	/*!< if archiving has been stopped,
					a thread can wait for this event to
					become signaled */
	/* @} */
#endif /* UNIV_LOG_ARCHIVE */
	ib_uint64_t	tracked_lsn;	/*!< log tracking has advanced to this
					lsn.  Field accessed atomically where
					64-bit atomic ops are supported,
					protected by the log sys mutex
					otherwise. */
};

/** Test if flush order mutex is owned. */
#define log_flush_order_mutex_own()	\
	mutex_own(&log_sys->log_flush_order_mutex)

/** Acquire the flush order mutex. */
#define log_flush_order_mutex_enter() do {		\
	mutex_enter(&log_sys->log_flush_order_mutex);	\
} while (0)
/** Release the flush order mutex. */
# define log_flush_order_mutex_exit() do {		\
	mutex_exit(&log_sys->log_flush_order_mutex);	\
} while (0)

#ifdef UNIV_LOG_ARCHIVE
/** Archiving state @{ */
#define LOG_ARCH_ON		71
#define LOG_ARCH_STOPPING	72
#define LOG_ARCH_STOPPING2	73
#define LOG_ARCH_STOPPED	74
#define LOG_ARCH_OFF		75
/* @} */
#endif /* UNIV_LOG_ARCHIVE */

#ifndef UNIV_NONINL
#include "log0log.ic"
#endif

#endif
