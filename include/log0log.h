/******************************************************
Database log

(c) 1995 Innobase Oy

Created 12/9/1995 Heikki Tuuri
*******************************************************/

#ifndef log0log_h
#define log0log_h

#include "univ.i"
#include "ut0byte.h"
#include "sync0sync.h"
#include "sync0rw.h"

typedef struct log_struct	log_t;
typedef struct log_group_struct	log_group_t;

#ifdef UNIV_DEBUG
extern	ibool	log_do_write;
extern 	ibool	log_debug_writes;
#else /* UNIV_DEBUG */
# define log_do_write TRUE
#endif /* UNIV_DEBUG */

/* Wait modes for log_write_up_to */
#define LOG_NO_WAIT		91
#define LOG_WAIT_ONE_GROUP	92
#define	LOG_WAIT_ALL_GROUPS	93
#define LOG_MAX_N_GROUPS	32

/********************************************************************
Sets the global variable log_fsp_current_free_limit. Also makes a checkpoint,
so that we know that the limit has been written to a log checkpoint field
on disk. */

void
log_fsp_current_free_limit_set_and_checkpoint(
/*==========================================*/
	ulint	limit);	/* in: limit to set */
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
	ib_longlong	log_file_size);		/* in: log file size
						(including the header) */
/****************************************************************
Writes to the log the string given. The log must be released with
log_release. */
UNIV_INLINE
dulint
log_reserve_and_write_fast(
/*=======================*/
			/* out: end lsn of the log record, ut_dulint_zero if
			did not succeed */
	byte*	str,	/* in: string */
	ulint	len,	/* in: string length */
	dulint*	start_lsn,/* out: start lsn of the log record */
	ibool*	success);/* out: TRUE if success */
/***************************************************************************
Releases the log mutex. */
UNIV_INLINE
void
log_release(void);
/*=============*/
/***************************************************************************
Checks if there is need for a log buffer flush or a new checkpoint, and does
this if yes. Any database operation should call this when it has modified
more than about 4 pages. NOTE that this function may only be called when the
OS thread owns no synchronization objects except the dictionary mutex. */
UNIV_INLINE
void
log_free_check(void);
/*================*/
/****************************************************************
Opens the log for log_write_low. The log must be closed with log_close and
released with log_release. */

dulint
log_reserve_and_open(
/*=================*/
			/* out: start lsn of the log record */
	ulint	len);	/* in: length of data to be catenated */
/****************************************************************
Writes to the log the string given. It is assumed that the caller holds the
log mutex. */

void
log_write_low(
/*==========*/
	byte*	str,		/* in: string */
	ulint	str_len);	/* in: string length */
/****************************************************************
Closes the log. */

dulint
log_close(void);
/*===========*/
			/* out: lsn */
/****************************************************************
Gets the current lsn. */
UNIV_INLINE
dulint
log_get_lsn(void);
/*=============*/
			/* out: current lsn */
/**********************************************************
Initializes the log. */

void
log_init(void);
/*==========*/
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
	ulint	archive_space_id);	/* in: space id of the file space
					which contains some archived log
					files for this group; currently, only
					for the first log group this is
					used */
/**********************************************************
Completes an i/o to a log file. */

void
log_io_complete(
/*============*/
	log_group_t*	group);	/* in: log group */
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
	ibool	flush_to_disk);
			/* in: TRUE if we want the written log also to be
			flushed to disk */
/********************************************************************
Does a syncronous flush of the log buffer to disk. */

void
log_buffer_flush_to_disk(void);
/*==========================*/
/********************************************************************
Advances the smallest lsn for which there are unflushed dirty blocks in the
buffer pool and also may make a new checkpoint. NOTE: this function may only
be called if the calling thread owns no synchronization objects! */

ibool
log_preflush_pool_modified_pages(
/*=============================*/
				/* out: FALSE if there was a flush batch of
				the same type running, which means that we
				could not start this flush batch */
	dulint	new_oldest,	/* in: try to advance oldest_modified_lsn
				at least to this lsn */
	ibool	sync);		/* in: TRUE if synchronous operation is
				desired */
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
	ibool	write_always);	/* in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
/********************************************************************
Makes a checkpoint at a given lsn or later. */

void
log_make_checkpoint_at(
/*===================*/
	dulint	lsn,		/* in: make a checkpoint at this or a later
				lsn, if ut_dulint_max, makes a checkpoint at
				the latest lsn */
	ibool	write_always);	/* in: the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files */
/********************************************************************
Makes a checkpoint at the latest lsn and writes it to first page of each
data file in the database, so that we know that the file spaces contain
all modifications up to that lsn. This can only be called at database
shutdown. This function also writes all log in log files to the log archive. */

void
logs_empty_and_mark_files_at_shutdown(void);
/*=======================================*/
/**********************************************************
Reads a checkpoint info from a log group header to log_sys->checkpoint_buf. */

void
log_group_read_checkpoint_info(
/*===========================*/
	log_group_t*	group,	/* in: log group */
	ulint		field);	/* in: LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2 */
/***********************************************************************
Gets info from a checkpoint about a log group. */

void
log_checkpoint_get_nth_group_info(
/*==============================*/
	byte*	buf,	/* in: buffer containing checkpoint info */
	ulint	n,	/* in: nth slot */
	ulint*	file_no,/* out: archived file number */
	ulint*	offset);/* out: archived file offset */
/**********************************************************
Writes checkpoint info to groups. */

void
log_groups_write_checkpoint_info(void);
/*==================================*/
/**********************************************************
Writes info to a buffer of a log group when log files are created in
backup restoration. */

void
log_reset_first_header_and_checkpoint(
/*==================================*/
	byte*	hdr_buf,/* in: buffer which will be written to the start
			of the first log file */
	dulint	start);	/* in: lsn of the start of the first log file;
			we pretend that there is a checkpoint at
			start + LOG_BLOCK_HDR_SIZE */
/************************************************************************
Starts an archiving operation. */

ibool
log_archive_do(
/*===========*/
			/* out: TRUE if succeed, FALSE if an archiving
			operation was already running */
	ibool	sync,	/* in: TRUE if synchronous operation is desired */
	ulint*	n_bytes);/* out: archive log buffer size, 0 if nothing to
			archive */
/********************************************************************
Writes the log contents to the archive up to the lsn when this function was
called, and stops the archiving. When archiving is started again, the archived
log file numbers start from a number one higher, so that the archiving will
not write again to the archived log files which exist when this function
returns. */

ulint
log_archive_stop(void);
/*==================*/
				/* out: DB_SUCCESS or DB_ERROR */
/********************************************************************
Starts again archiving which has been stopped. */

ulint
log_archive_start(void);
/*===================*/
			/* out: DB_SUCCESS or DB_ERROR */
/********************************************************************
Stop archiving the log so that a gap may occur in the archived log files. */

ulint
log_archive_noarchivelog(void);
/*==========================*/
			/* out: DB_SUCCESS or DB_ERROR */
/********************************************************************
Start archiving the log so that a gap may occur in the archived log files. */

ulint
log_archive_archivelog(void);
/*========================*/
			/* out: DB_SUCCESS or DB_ERROR */
/**********************************************************
Generates an archived log file name. */

void
log_archived_file_name_gen(
/*=======================*/
	char*	buf,	/* in: buffer where to write */
	ulint	id,	/* in: group id */
	ulint	file_no);/* in: file number */
/************************************************************************
Checks that there is enough free space in the log to start a new query step.
Flushes the log buffer or makes a new checkpoint if necessary. NOTE: this
function may only be called if the calling thread owns no synchronization
objects! */

void
log_check_margins(void);
/*===================*/
/**********************************************************
Reads a specified log segment to a buffer. */

void
log_group_read_log_seg(
/*===================*/
	ulint		type,		/* in: LOG_ARCHIVE or LOG_RECOVER */
	byte*		buf,		/* in: buffer where to read */
	log_group_t*	group,		/* in: log group */
	dulint		start_lsn,	/* in: read area start */
	dulint		end_lsn);	/* in: read area end */
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
	ulint		new_data_offset);/* in: start offset of new data in
					buf: this parameter is used to decide
					if we have to write a new log file
					header */
/************************************************************
Sets the field values in group to correspond to a given lsn. For this function
to work, the values must already be correctly initialized to correspond to
some lsn, for instance, a checkpoint lsn. */

void
log_group_set_fields(
/*=================*/
	log_group_t*	group,	/* in: group */
	dulint		lsn);	/* in: lsn for which the values should be
				set */
/**********************************************************
Calculates the data capacity of a log group, when the log file headers are not
included. */

ulint
log_group_get_capacity(
/*===================*/
				/* out: capacity in bytes */
	log_group_t*	group);	/* in: log group */
/****************************************************************
Gets a log block flush bit. */
UNIV_INLINE
ibool
log_block_get_flush_bit(
/*====================*/
				/* out: TRUE if this block was the first
				to be written in a log flush */
	byte*	log_block);	/* in: log block */
/****************************************************************
Gets a log block number stored in the header. */
UNIV_INLINE
ulint
log_block_get_hdr_no(
/*=================*/
				/* out: log block number stored in the block
				header */
	byte*	log_block);	/* in: log block */
/****************************************************************
Gets a log block data length. */
UNIV_INLINE
ulint
log_block_get_data_len(
/*===================*/
				/* out: log block data length measured as a
				byte offset from the block start */
	byte*	log_block);	/* in: log block */
/****************************************************************
Sets the log block data length. */
UNIV_INLINE
void
log_block_set_data_len(
/*===================*/
	byte*	log_block,	/* in: log block */
	ulint	len);		/* in: data length */
/****************************************************************
Calculates the checksum for a log block. */
UNIV_INLINE
ulint
log_block_calc_checksum(
/*====================*/
			/* out: checksum */
	byte*	block);	/* in: log block */
/****************************************************************
Gets a log block checksum field value. */
UNIV_INLINE
ulint
log_block_get_checksum(
/*===================*/
				/* out: checksum */
	byte*	log_block);	/* in: log block */
/****************************************************************
Sets a log block checksum field value. */
UNIV_INLINE
void
log_block_set_checksum(
/*===================*/
	byte*	log_block,	/* in: log block */
	ulint	checksum);	/* in: checksum */
/****************************************************************
Gets a log block first mtr log record group offset. */
UNIV_INLINE
ulint
log_block_get_first_rec_group(
/*==========================*/
				/* out: first mtr log record group byte offset
				from the block start, 0 if none */
	byte*	log_block);	/* in: log block */
/****************************************************************
Sets the log block first mtr log record group offset. */
UNIV_INLINE
void
log_block_set_first_rec_group(
/*==========================*/
	byte*	log_block,	/* in: log block */
	ulint	offset);	/* in: offset, 0 if none */
/****************************************************************
Gets a log block checkpoint number field (4 lowest bytes). */
UNIV_INLINE
ulint
log_block_get_checkpoint_no(
/*========================*/
				/* out: checkpoint no (4 lowest bytes) */
	byte*	log_block);	/* in: log block */
/****************************************************************
Initializes a log block in the log buffer. */
UNIV_INLINE
void
log_block_init(
/*===========*/
	byte*	log_block,	/* in: pointer to the log buffer */
	dulint	lsn);		/* in: lsn within the log block */
/****************************************************************
Initializes a log block in the log buffer in the old, < 3.23.52 format, where
there was no checksum yet. */
UNIV_INLINE
void
log_block_init_in_old_format(
/*=========================*/
	byte*	log_block,	/* in: pointer to the log buffer */
	dulint	lsn);		/* in: lsn within the log block */
/****************************************************************
Converts a lsn to a log block number. */
UNIV_INLINE
ulint
log_block_convert_lsn_to_no(
/*========================*/
			/* out: log block number, it is > 0 and <= 1G */
	dulint	lsn);	/* in: lsn of a byte within the block */
/**********************************************************
Prints info of the log. */

void
log_print(
/*======*/
	FILE*	file);	/* in: file where to print */
/**********************************************************
Peeks the current lsn. */

ibool
log_peek_lsn(
/*=========*/
                       /* out: TRUE if success, FALSE if could not get the
                       log system mutex */
       dulint* lsn);   /* out: if returns TRUE, current lsn is here */
/**************************************************************************
Refreshes the statistics used to print per-second averages. */

void
log_refresh_stats(void);
/*===================*/

extern log_t*	log_sys;

/* Values used as flags */
#define LOG_FLUSH	7652559
#define LOG_CHECKPOINT	78656949
#define LOG_ARCHIVE	11122331
#define LOG_RECOVER	98887331

/* The counting of lsn's starts from this value: this must be non-zero */
#define LOG_START_LSN	ut_dulint_create(0, 16 * OS_FILE_LOG_BLOCK_SIZE)

#define LOG_BUFFER_SIZE 	(srv_log_buffer_size * UNIV_PAGE_SIZE)
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

/* For each value < LOG_MAX_N_GROUPS the following 8 bytes: */

#define LOG_CHECKPOINT_ARCHIVED_FILE_NO	0
#define LOG_CHECKPOINT_ARCHIVED_OFFSET	4

#define	LOG_CHECKPOINT_ARRAY_END	(LOG_CHECKPOINT_GROUP_ARRAY\
							+ LOG_MAX_N_GROUPS * 8)
#define LOG_CHECKPOINT_CHECKSUM_1 	LOG_CHECKPOINT_ARRAY_END
#define LOG_CHECKPOINT_CHECKSUM_2 	(4 + LOG_CHECKPOINT_ARRAY_END)
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

/* Log group consists of a number of log files, each of the same size; a log
group is implemented as a space in the sense of the module fil0fil. */

struct log_group_struct{
	/* The following fields are protected by log_sys->mutex */
	ulint		id;		/* log group id */
	ulint		n_files;	/* number of files in the group */
	ulint		file_size;	/* individual log file size in bytes,
					including the log file header */
	ulint		space_id;	/* file space which implements the log
					group */
	ulint		state;		/* LOG_GROUP_OK or
					LOG_GROUP_CORRUPTED */
	dulint		lsn;		/* lsn used to fix coordinates within
					the log group */
	ulint		lsn_offset;	/* the offset of the above lsn */
	ulint		n_pending_writes;/* number of currently pending flush
					writes for this log group */
	byte**		file_header_bufs;/* buffers for each file header in the
					group */
	/*-----------------------------*/
	byte**		archive_file_header_bufs;/* buffers for each file
					header in the group */
	ulint		archive_space_id;/* file space which implements the log
					group archive */
	ulint		archived_file_no;/* file number corresponding to
					log_sys->archived_lsn */
	ulint		archived_offset;/* file offset corresponding to
					log_sys->archived_lsn, 0 if we have
					not yet written to the archive file
					number archived_file_no */
	ulint		next_archived_file_no;/* during an archive write,
					until the write is completed, we
					store the next value for
					archived_file_no here: the write
					completion function then sets the new
					value to ..._file_no */
	ulint		next_archived_offset; /* like the preceding field */
	/*-----------------------------*/
	dulint		scanned_lsn;	/* used only in recovery: recovery scan
					succeeded up to this lsn in this log
					group */
	byte*		checkpoint_buf;	/* checkpoint header is written from
					this buffer to the group */
	UT_LIST_NODE_T(log_group_t)
			log_groups;	/* list of log groups */
};	

struct log_struct{
	byte		pad[64];	/* padding to prevent other memory
					update hotspots from residing on the
					same memory cache line */
	dulint		lsn;		/* log sequence number */
	ulint		buf_free;	/* first free offset within the log
					buffer */
	mutex_t		mutex;		/* mutex protecting the log */
	byte*		buf;		/* log buffer */
	ulint		buf_size;	/* log buffer size in bytes */
	ulint		max_buf_free;	/* recommended maximum value of
					buf_free, after which the buffer is
					flushed */
	ulint		old_buf_free;	/* value of buf free when log was
					last time opened; only in the debug
					version */
	dulint		old_lsn;	/* value of lsn when log was last time
					opened; only in the debug version */
	ibool		check_flush_or_checkpoint;
					/* this is set to TRUE when there may
					be need to flush the log buffer, or
					preflush buffer pool pages, or make
					a checkpoint; this MUST be TRUE when
					lsn - last_checkpoint_lsn >
					max_checkpoint_age; this flag is
					peeked at by log_free_check(), which
					does not reserve the log mutex */
	UT_LIST_BASE_NODE_T(log_group_t)
			log_groups;	/* log groups */

	/* The fields involved in the log buffer flush */

	ulint		buf_next_to_write;/* first offset in the log buffer
					where the byte content may not exist
					written to file, e.g., the start
					offset of a log record catenated
					later; this is advanced when a flush
					operation is completed to all the log
					groups */
	dulint		written_to_some_lsn;
					/* first log sequence number not yet
					written to any log group; for this to
					be advanced, it is enough that the
					write i/o has been completed for any
					one log group */
	dulint		written_to_all_lsn;
					/* first log sequence number not yet
					written to some log group; for this to
					be advanced, it is enough that the
					write i/o has been completed for all
					log groups */
	dulint		write_lsn;	/* end lsn for the current running 
					write */
	ulint		write_end_offset;/* the data in buffer has been written
					up to this offset when the current
					write ends: this field will then
					be copied to buf_next_to_write */
	dulint		current_flush_lsn;/* end lsn for the current running 
					write + flush operation */
	dulint		flushed_to_disk_lsn;
					/* how far we have written the log
					AND flushed to disk */
	ulint		n_pending_writes;/* number of currently pending flushes
					or writes */
	/* NOTE on the 'flush' in names of the fields below: starting from
	4.0.14, we separate the write of the log file and the actual fsync()
	or other method to flush it to disk. The names below shhould really
	be 'flush_or_write'! */
	os_event_t	no_flush_event;	/* this event is in the reset state
					when a flush or a write is running;
					a thread should wait for this without
					owning the log mutex, but NOTE that
					to set or reset this event, the
					thread MUST own the log mutex! */
	ibool		one_flushed;	/* during a flush, this is first FALSE
					and becomes TRUE when one log group
					has been written or flushed */
	os_event_t	one_flushed_event;/* this event is reset when the
					flush or write has not yet completed
					for any log group; e.g., this means
					that a transaction has been committed
					when this is set; a thread should wait
					for this without owning the log mutex,
					but NOTE that to set or reset this
					event, the thread MUST own the log
					mutex! */
	ulint		n_log_ios;	/* number of log i/os initiated thus
					far */
	ulint		n_log_ios_old;	/* number of log i/o's at the
					previous printout */
	time_t		last_printout_time;/* when log_print was last time
					called */

	/* Fields involved in checkpoints */
        ulint           log_group_capacity; /* capacity of the log group; if
                                        the checkpoint age exceeds this, it is
                                        a serious error because it is possible
                                        we will then overwrite log and spoil
                                        crash recovery */
	ulint		max_modified_age_async;
					/* when this recommended value for lsn
					- buf_pool_get_oldest_modification()
					is exceeded, we start an asynchronous
					preflush of pool pages */
	ulint		max_modified_age_sync;
					/* when this recommended value for lsn
					- buf_pool_get_oldest_modification()
					is exceeded, we start a synchronous
					preflush of pool pages */
	ulint		adm_checkpoint_interval;
					/* administrator-specified checkpoint
					interval in terms of log growth in
					bytes; the interval actually used by
					the database can be smaller */
	ulint		max_checkpoint_age_async;
					/* when this checkpoint age is exceeded
					we start an asynchronous writing of a
					new checkpoint */
	ulint		max_checkpoint_age;
					/* this is the maximum allowed value
					for lsn - last_checkpoint_lsn when a
					new query step is started */
	dulint		next_checkpoint_no;
					/* next checkpoint number */
	dulint		last_checkpoint_lsn;
					/* latest checkpoint lsn */
	dulint		next_checkpoint_lsn;
					/* next checkpoint lsn */
	ulint		n_pending_checkpoint_writes;
					/* number of currently pending
					checkpoint writes */
	rw_lock_t	checkpoint_lock;/* this latch is x-locked when a
					checkpoint write is running; a thread
					should wait for this without owning
					the log mutex */
	byte*		checkpoint_buf;	/* checkpoint header is read to this
					buffer */
	/* Fields involved in archiving */
	ulint		archiving_state;/* LOG_ARCH_ON, LOG_ARCH_STOPPING
					LOG_ARCH_STOPPED, LOG_ARCH_OFF */
	dulint		archived_lsn;	/* archiving has advanced to this
					lsn */
	ulint		max_archived_lsn_age_async;
					/* recommended maximum age of
					archived_lsn, before we start
					asynchronous copying to the archive */
	ulint		max_archived_lsn_age;
					/* maximum allowed age for
					archived_lsn */
	dulint		next_archived_lsn;/* during an archive write,
					until the write is completed, we
					store the next value for
					archived_lsn here: the write
					completion function then sets the new
					value to archived_lsn */
	ulint		archiving_phase;/* LOG_ARCHIVE_READ or
					LOG_ARCHIVE_WRITE */
	ulint		n_pending_archive_ios;
					/* number of currently pending reads
					or writes in archiving */
	rw_lock_t	archive_lock;	/* this latch is x-locked when an
					archive write is running; a thread
					should wait for this without owning
					the log mutex */
	ulint		archive_buf_size;/* size of archive_buf */
	byte*		archive_buf;	/* log segment is written to the
					archive from this buffer */
	os_event_t	archiving_on;	/* if archiving has been stopped,
					a thread can wait for this event to
					become signaled */
};

#define LOG_ARCH_ON		71
#define LOG_ARCH_STOPPING	72
#define LOG_ARCH_STOPPING2	73
#define LOG_ARCH_STOPPED	74
#define LOG_ARCH_OFF		75

#ifndef UNIV_NONINL
#include "log0log.ic"
#endif

#endif
