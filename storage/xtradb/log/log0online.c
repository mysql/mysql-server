/*****************************************************************************

Copyright (c) 2011-2012 Percona Inc. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file log/log0online.c
Online database log parsing for changed page tracking

*******************************************************/

#include "log0online.h"

#include "my_dbug.h"

#include "log0recv.h"
#include "mach0data.h"
#include "mtr0log.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0sys.h"
#include "ut0rbt.h"

enum { FOLLOW_SCAN_SIZE = 4 * (UNIV_PAGE_SIZE_MAX) };

/** Log parsing and bitmap output data structure */
struct log_bitmap_struct {
	byte		read_buf[FOLLOW_SCAN_SIZE];
					/*!< log read buffer */
	byte		parse_buf[RECV_PARSING_BUF_SIZE];
					/*!< log parse buffer */
	byte*		parse_buf_end;  /*!< parse buffer position where the
					next read log data should be copied to.
					If the previous log records were fully
					parsed, it points to the start,
					otherwise points immediatelly past the
					end of the incomplete log record. */
	char*		out_name;	/*!< the file name for bitmap output */
	os_file_t	out;		/*!< the bitmap output file */
	ib_uint64_t	out_offset;	/*!< the next write position in the
					bitmap output file */
	ib_uint64_t	start_lsn;	/*!< the LSN of the next unparsed
					record and the start of the next LSN
					interval to be parsed.  */
	ib_uint64_t	end_lsn;	/*!< the end of the LSN interval to be
					parsed, equal to the next checkpoint
					LSN at the time of parse */
	ib_uint64_t	next_parse_lsn;	/*!< the LSN of the next unparsed
					record in the current parse */
	ib_rbt_t*	modified_pages; /*!< the current modified page set,
					organized as the RB-tree with the keys
					of (space, 4KB-block-start-page-id)
					pairs */
	ib_rbt_node_t*	page_free_list; /*!< Singly-linked list of freed nodes
					of modified_pages tree for later
					reuse.  Nodes are linked through
					ib_rbt_node_t.left as this field has
					both the correct type and the tree does
					not mind its overwrite during
					rbt_next() tree traversal. */
};

/* The log parsing and bitmap output struct instance */
static struct log_bitmap_struct* log_bmp_sys;

/* File name stem for modified page bitmaps */
static const char* modified_page_stem = "ib_modified_log.";

/* On server startup with empty database srv_start_lsn == 0, in
which case the first LSN of actual log records will be this. */
#define MIN_TRACKED_LSN ((LOG_START_LSN) + (LOG_BLOCK_HDR_SIZE))

/* Tests if num bit of bitmap is set */
#define IS_BIT_SET(bitmap, num) \
        (*((bitmap) + ((num) >> 3)) & (1UL << ((num) & 7UL)))

/** The bitmap file block size in bytes.  All writes will be multiples of this.
 */
enum {
	MODIFIED_PAGE_BLOCK_SIZE = 4096
};


/** Offsets in a file bitmap block */
enum {
	MODIFIED_PAGE_IS_LAST_BLOCK = 0,/* 1 if last block in the current
					write, 0 otherwise. */
	MODIFIED_PAGE_START_LSN = 4,	/* The starting tracked LSN of this and
					other blocks in the same write */
	MODIFIED_PAGE_END_LSN = 12,	/* The ending tracked LSN of this and
					other blocks in the same write */
	MODIFIED_PAGE_SPACE_ID = 20,	/* The space ID of tracked pages in
					this block */
	MODIFIED_PAGE_1ST_PAGE_ID = 24,	/* The page ID of the first tracked
					page in this block */
	MODIFIED_PAGE_BLOCK_UNUSED_1 = 28,/* Unused in order to align the start
					of bitmap at 8 byte boundary */
	MODIFIED_PAGE_BLOCK_BITMAP = 32,/* Start of the bitmap itself */
	MODIFIED_PAGE_BLOCK_UNUSED_2 = MODIFIED_PAGE_BLOCK_SIZE - 8,
					/* Unused in order to align the end of
					bitmap at 8 byte boundary */
	MODIFIED_PAGE_BLOCK_CHECKSUM = MODIFIED_PAGE_BLOCK_SIZE - 4
					/* The checksum of the current block */
};

/** Length of the bitmap data in a block in bytes */
enum { MODIFIED_PAGE_BLOCK_BITMAP_LEN
       = MODIFIED_PAGE_BLOCK_UNUSED_2 - MODIFIED_PAGE_BLOCK_BITMAP };

/** Length of the bitmap data in a block in page ids */
enum { MODIFIED_PAGE_BLOCK_ID_COUNT = MODIFIED_PAGE_BLOCK_BITMAP_LEN * 8 };

/****************************************************************//**
Provide a comparisson function for the RB-tree tree (space,
block_start_page) pairs.  Actual implementation does not matter as
long as the ordering is full.
@return -1 if p1 < p2, 0 if p1 == p2, 1 if p1 > p2
*/
static
int
log_online_compare_bmp_keys(
/*========================*/
	const void* p1,	/*!<in: 1st key to compare */
	const void* p2)	/*!<in: 2nd key to compare */
{
	const byte *k1 = (const byte *)p1;
	const byte *k2 = (const byte *)p2;

	ulint k1_space = mach_read_from_4(k1 + MODIFIED_PAGE_SPACE_ID);
	ulint k2_space = mach_read_from_4(k2 + MODIFIED_PAGE_SPACE_ID);
	if (k1_space == k2_space) {
		ulint k1_start_page
			= mach_read_from_4(k1 + MODIFIED_PAGE_1ST_PAGE_ID);
		ulint k2_start_page
			= mach_read_from_4(k2 + MODIFIED_PAGE_1ST_PAGE_ID);
		return k1_start_page < k2_start_page
			? -1 : k1_start_page > k2_start_page ? 1 : 0;
	}
	return k1_space < k2_space ? -1 : 1;
}

/****************************************************************//**
Set a bit for tracked page in the bitmap. Expand the bitmap tree as
necessary. */
static
void
log_online_set_page_bit(
/*====================*/
	ulint	space,	/*!<in: log record space id */
	ulint	page_no)/*!<in: log record page id */
{
	ulint		block_start_page;
	ulint		block_pos;
	uint		bit_pos;
	ib_rbt_bound_t	tree_search_pos;
	byte		search_page[MODIFIED_PAGE_BLOCK_SIZE];
	byte		*page_ptr;

	ut_a(space != ULINT_UNDEFINED);
	ut_a(page_no != ULINT_UNDEFINED);

	block_start_page = page_no / MODIFIED_PAGE_BLOCK_ID_COUNT
		* MODIFIED_PAGE_BLOCK_ID_COUNT;
	block_pos = block_start_page ? (page_no % block_start_page / 8)
		: (page_no / 8);
	bit_pos = page_no % 8;

	mach_write_to_4(search_page + MODIFIED_PAGE_SPACE_ID, space);
	mach_write_to_4(search_page + MODIFIED_PAGE_1ST_PAGE_ID,
			block_start_page);

	if (!rbt_search(log_bmp_sys->modified_pages, &tree_search_pos,
			search_page)) {
		page_ptr = rbt_value(byte, tree_search_pos.last);
	}
	else {
		ib_rbt_node_t *new_node;

		if (log_bmp_sys->page_free_list) {
			new_node = log_bmp_sys->page_free_list;
			log_bmp_sys->page_free_list = new_node->left;
		}
		else {
			new_node = ut_malloc(SIZEOF_NODE(
				  log_bmp_sys->modified_pages));
		}
		memset(new_node, 0, SIZEOF_NODE(log_bmp_sys->modified_pages));

		page_ptr = rbt_value(byte, new_node);
		mach_write_to_4(page_ptr + MODIFIED_PAGE_SPACE_ID, space);
		mach_write_to_4(page_ptr + MODIFIED_PAGE_1ST_PAGE_ID,
				block_start_page);

		rbt_add_preallocated_node(log_bmp_sys->modified_pages,
					  &tree_search_pos, new_node);
	}
	page_ptr[MODIFIED_PAGE_BLOCK_BITMAP + block_pos] |= (1U << bit_pos);
}

/****************************************************************//**
Calculate a bitmap block checksum.  Algorithm borrowed from
log_block_calc_checksum.
@return checksum */
UNIV_INLINE
ulint
log_online_calc_checksum(
/*=====================*/
	const byte*	block)	/*!<in: bitmap block */
{
	ulint	sum;
	ulint	sh;
	ulint	i;

	sum = 1;
	sh = 0;

	for (i = 0; i < MODIFIED_PAGE_BLOCK_CHECKSUM; i++) {

		ulint	b = block[i];
		sum &= 0x7FFFFFFFUL;
		sum += b;
		sum += b << sh;
		sh++;
		if (sh > 24) {
			sh = 0;
		}
	}

	return sum;
}

/****************************************************************//**
Get the last tracked fully LSN from the bitmap file by reading
backwards untile a correct end page is found.  Detects incomplete
writes and corrupted data.  Sets the start output position for the
written bitmap data.
@return the last fully tracked LSN */
static
ib_uint64_t
log_online_read_last_tracked_lsn()
/*==============================*/
{
	byte		page[MODIFIED_PAGE_BLOCK_SIZE];
	ib_uint64_t	read_offset	= log_bmp_sys->out_offset;
	/* Initialize these to nonequal values so that file size == 0 case with
	zero loop repetitions is handled correctly */
	ulint		checksum	= 0;
	ulint		actual_checksum = !checksum;
	ibool		is_last_page	= FALSE;
	ib_uint64_t	result;

	ut_ad(log_bmp_sys->out_offset % MODIFIED_PAGE_BLOCK_SIZE == 0);

	while (checksum != actual_checksum && read_offset > 0 && !is_last_page)
	{

		ulint		offset_low, offset_high;
		ibool		success;

		read_offset -= MODIFIED_PAGE_BLOCK_SIZE;
		offset_high = (ulint)(read_offset >> 32);
		offset_low = (ulint)(read_offset & 0xFFFFFFFF);

		success = os_file_read(log_bmp_sys->out, page, offset_low,
				       offset_high, MODIFIED_PAGE_BLOCK_SIZE);
		if (!success) {

			/* The following call prints an error message */
			os_file_get_last_error(TRUE);
			/* Here and below assume that bitmap file names do not
			contain apostrophes, thus no need for
			ut_print_filename(). */
			fprintf(stderr, "InnoDB: Warning: failed reading "
				"changed page bitmap file \'%s\'\n",
				log_bmp_sys->out_name);
			return MIN_TRACKED_LSN;
		}

		is_last_page
			= mach_read_from_4(page + MODIFIED_PAGE_IS_LAST_BLOCK);
		checksum = mach_read_from_4(page
					    + MODIFIED_PAGE_BLOCK_CHECKSUM);
		actual_checksum = log_online_calc_checksum(page);
		if (checksum != actual_checksum) {

			fprintf(stderr, "InnoDB: Warning: corruption "
				"detected in \'%s\' at offset %llu\n",
				log_bmp_sys->out_name, read_offset);
		}

	};

	if (UNIV_LIKELY(checksum == actual_checksum && is_last_page)) {

		log_bmp_sys->out_offset = read_offset
			+ MODIFIED_PAGE_BLOCK_SIZE;
		result = mach_read_ull(page + MODIFIED_PAGE_END_LSN);
	}
	else {
		log_bmp_sys->out_offset = read_offset;
		result = 0;
	}

	/* Truncate the output file to discard the corrupted bitmap data, if
	any */
	if (!os_file_set_eof_at(log_bmp_sys->out,
				log_bmp_sys->out_offset)) {
		fprintf(stderr, "InnoDB: Warning: failed truncating "
			"changed page bitmap file \'%s\' to %llu bytes\n",
			log_bmp_sys->out_name, log_bmp_sys->out_offset);
		result = 0;
	}
	return result;
}

/****************************************************************//**
Safely write the log_sys->tracked_lsn value.  Uses atomic operations
if available, otherwise this field is protected with the log system
mutex.  The reader counterpart function is log_get_tracked_lsn() in
log0log.c. */
UNIV_INLINE
void
log_set_tracked_lsn(
/*================*/
	ib_uint64_t	tracked_lsn)	/*!<in: new value */
{
#ifdef HAVE_ATOMIC_BUILTINS_64
	/* Single writer, no data race here */
	ib_uint64_t old_value
		= os_atomic_increment_uint64(&log_sys->tracked_lsn, 0);
	(void) os_atomic_increment_uint64(&log_sys->tracked_lsn,
					  tracked_lsn - old_value);
#else
	mutex_enter(&log_sys->mutex);
	log_sys->tracked_lsn = tracked_lsn;
	mutex_exit(&log_sys->mutex);
#endif
}

/****************************************************************//**
Diagnose a gap in tracked LSN range on server startup due to crash or
very fast shutdown and try to close it by tracking the data
immediatelly, if possible. */
static
void
log_online_track_missing_on_startup(
/*================================*/
	ib_uint64_t	last_tracked_lsn,	/*!<in: last tracked LSN read
						from the bitmap file */
	ib_uint64_t	tracking_start_lsn)	/*!<in: last checkpoint LSN of
						the current server startup */
{
	ut_ad(last_tracked_lsn != tracking_start_lsn);

	fprintf(stderr, "InnoDB: last tracked LSN in \'%s\' is %llu, but "
		"last checkpoint LSN is %llu.  This might be due to a server "
		"crash or a very fast shutdown.  ", log_bmp_sys->out_name,
		last_tracked_lsn, tracking_start_lsn);

	/* last_tracked_lsn might be < MIN_TRACKED_LSN in the case of empty
	   bitmap file, handle this too. */
	last_tracked_lsn = ut_max(last_tracked_lsn, MIN_TRACKED_LSN);

	/* See if we can fully recover the missing interval */
	if (log_sys->lsn - last_tracked_lsn < log_sys->log_group_capacity) {

		fprintf(stderr,
			"Reading the log to advance the last tracked LSN.\n");

		log_bmp_sys->start_lsn = last_tracked_lsn;
		log_set_tracked_lsn(log_bmp_sys->start_lsn);
		log_online_follow_redo_log();
		ut_ad(log_bmp_sys->end_lsn >= tracking_start_lsn);

		fprintf(stderr,
			"InnoDB: continuing tracking changed pages from LSN "
			"%llu\n", log_bmp_sys->end_lsn);
	}
	else {
		fprintf(stderr,
			"The age of last tracked LSN exceeds log capacity, "
			"tracking-based incremental backups will work only "
			"from the higher LSN!\n");

		log_bmp_sys->end_lsn = log_bmp_sys->start_lsn
			= tracking_start_lsn;
		log_set_tracked_lsn(log_bmp_sys->start_lsn);

		fprintf(stderr,
			"InnoDB: starting tracking changed pages from LSN "
			"%llu\n", log_bmp_sys->end_lsn);
	}
}

/*********************************************************************//**
Initialize the online log following subsytem. */
UNIV_INTERN
void
log_online_read_init()
/*==================*/
{
	char		buf[FN_REFLEN];
	ibool		success;
	ib_uint64_t	tracking_start_lsn
		= ut_max(log_sys->last_checkpoint_lsn, MIN_TRACKED_LSN);

	/* Assert (could be compile-time assert) that bitmap data start and end
	in a bitmap block is 8-byte aligned */
	ut_a(MODIFIED_PAGE_BLOCK_BITMAP % 8 == 0);
	ut_a(MODIFIED_PAGE_BLOCK_BITMAP_LEN % 8 == 0);

	log_bmp_sys = ut_malloc(sizeof(*log_bmp_sys));

	ut_snprintf(buf, FN_REFLEN, "%s%s%d", srv_data_home,
		    modified_page_stem, 1);
	log_bmp_sys->out_name = ut_malloc(strlen(buf) + 1);
	ut_strcpy(log_bmp_sys->out_name, buf);

	log_bmp_sys->modified_pages = rbt_create(MODIFIED_PAGE_BLOCK_SIZE,
						 log_online_compare_bmp_keys);
	log_bmp_sys->page_free_list = NULL;

	log_bmp_sys->out
		= os_file_create_simple_no_error_handling
		(log_bmp_sys->out_name, OS_FILE_OPEN, OS_FILE_READ_WRITE,
		 &success);

	if (!success) {

		/* New file, tracking from scratch */
		log_bmp_sys->out
			= os_file_create_simple_no_error_handling
			(log_bmp_sys->out_name,	OS_FILE_CREATE,
			 OS_FILE_READ_WRITE, &success);
		if (!success) {

			/* The following call prints an error message */
			os_file_get_last_error(TRUE);
			fprintf(stderr,
				"InnoDB: Error: Cannot create \'%s\'\n",
				log_bmp_sys->out_name);
			exit(1);
		}

		log_bmp_sys->out_offset = 0;
	}
	else {

		/* Old file, read last tracked LSN and continue from there */
		ulint		size_low;
		ulint		size_high;
		ib_uint64_t	last_tracked_lsn;

		success = os_file_get_size(log_bmp_sys->out, &size_low,
					   &size_high);
		ut_a(success);

		log_bmp_sys->out_offset
			= ((ib_uint64_t)size_high << 32) | size_low;

		if (log_bmp_sys->out_offset % MODIFIED_PAGE_BLOCK_SIZE != 0) {

			fprintf(stderr,
				"InnoDB: Warning: truncated block detected "
				"in \'%s\' at offset %llu\n",
				log_bmp_sys->out_name,
				log_bmp_sys->out_offset);
			log_bmp_sys->out_offset -=
				log_bmp_sys->out_offset
				% MODIFIED_PAGE_BLOCK_SIZE;
		}

		last_tracked_lsn = log_online_read_last_tracked_lsn();

		if (last_tracked_lsn < tracking_start_lsn) {

			log_online_track_missing_on_startup(last_tracked_lsn,
							    tracking_start_lsn);
			return;
		}

		if (last_tracked_lsn > tracking_start_lsn) {

			fprintf(stderr, "InnoDB: last tracked LSN in \'%s\' "
				"is %llu, but last checkpoint LSN is %llu. "
				"The tracking-based incremental backups will "
				"work only from the latter LSN!\n",
				log_bmp_sys->out_name, last_tracked_lsn,
				tracking_start_lsn);
		}

	}

	fprintf(stderr, "InnoDB: starting tracking changed pages from "
		"LSN %llu\n", tracking_start_lsn);
	log_bmp_sys->start_lsn = tracking_start_lsn;
	log_set_tracked_lsn(tracking_start_lsn);
}

/*********************************************************************//**
Shut down the online log following subsystem. */
UNIV_INTERN
void
log_online_read_shutdown()
/*======================*/
{
	ib_rbt_node_t *free_list_node = log_bmp_sys->page_free_list;

	os_file_close(log_bmp_sys->out);

	rbt_free(log_bmp_sys->modified_pages);

	while (free_list_node) {
		ib_rbt_node_t *next = free_list_node->left;
		ut_free(free_list_node);
		free_list_node = next;
	}

	ut_free(log_bmp_sys->out_name);
	ut_free(log_bmp_sys);
}

/*********************************************************************//**
For the given minilog record type determine if the record has (space; page)
associated with it.
@return TRUE if the record has (space; page) in it */
static
ibool
log_online_rec_has_page(
/*====================*/
	byte	type)	/*!<in: the minilog record type */
{
	return type != MLOG_MULTI_REC_END && type != MLOG_DUMMY_RECORD;
}

/*********************************************************************//**
Check if a page field for a given log record type actually contains a page
id. It does not for file operations and MLOG_LSN.
@return TRUE if page field contains actual page id, FALSE otherwise */
static
ibool
log_online_rec_page_means_page(
/*===========================*/
	byte	type)	/*!<in: log record type */
{
	return log_online_rec_has_page(type)
#ifdef UNIV_LOG_LSN_DEBUG
		&& type != MLOG_LSN
#endif
		&& type != MLOG_FILE_CREATE
		&& type != MLOG_FILE_RENAME
		&& type != MLOG_FILE_DELETE
		&& type != MLOG_FILE_CREATE2;
}

/*********************************************************************//**
Parse the log data in the parse buffer for the (space, page) pairs and add
them to the modified page set as necessary.  Removes the fully-parsed records
from the buffer.  If an incomplete record is found, moves it to the end of the
buffer. */
static
void
log_online_parse_redo_log()
/*=======================*/
{
	byte *ptr = log_bmp_sys->parse_buf;
	byte *end = log_bmp_sys->parse_buf_end;

	ulint len = 0;

	while (ptr != end
	       && log_bmp_sys->next_parse_lsn < log_bmp_sys->end_lsn) {

		byte	type;
		ulint	space;
		ulint	page_no;
		byte*	body;

		/* recv_sys is not initialized, so on corrupt log we will
		SIGSEGV.  But the log of a live database should not be
		corrupt. */
		len = recv_parse_log_rec(ptr, end, &type, &space, &page_no,
					 &body);
		if (len > 0) {

			if (log_online_rec_page_means_page(type)
			    && (space != TRX_DOUBLEWRITE_SPACE)) {

				ut_a(len >= 3);
				log_online_set_page_bit(space, page_no);
			}

			ptr += len;
			ut_ad(ptr <= end);
			log_bmp_sys->next_parse_lsn
			    = recv_calc_lsn_on_data_add
				(log_bmp_sys->next_parse_lsn, len);
		}
		else {

			/* Incomplete log record.  Shift it to the
			beginning of the parse buffer and leave it to be
			completed on the next read.  */
			ut_memmove(log_bmp_sys->parse_buf, ptr, end - ptr);
			log_bmp_sys->parse_buf_end
				= log_bmp_sys->parse_buf + (end - ptr);
			ptr = end;
		}
	}

	if (len > 0) {

		log_bmp_sys->parse_buf_end = log_bmp_sys->parse_buf;
	}
}

/*********************************************************************//**
Check the log block checksum.
@return TRUE if the log block checksum is OK, FALSE otherwise.  */
static
ibool
log_online_is_valid_log_seg(
/*========================*/
	const byte* log_block)	/*!< in: read log data */
{
	ibool checksum_is_ok
		= log_block_checksum_is_ok_or_old_format(log_block);

	if (!checksum_is_ok) {

		fprintf(stderr,
			"InnoDB Error: log block checksum mismatch"
			"expected %lu, calculated checksum %lu\n",
			(ulong) log_block_get_checksum(log_block),
			(ulong) log_block_calc_checksum(log_block));
	}

	return checksum_is_ok;
}

/*********************************************************************//**
Copy new log data to the parse buffer while skipping log block header,
trailer and already parsed data.  */
static
void
log_online_add_to_parse_buf(
/*========================*/
	const byte*	log_block,	/*!< in: read log data */
	ulint		data_len,	/*!< in: length of read log data */
	ulint		skip_len)	/*!< in: how much of log data to
					skip */
{
	ulint start_offset = skip_len ? skip_len : LOG_BLOCK_HDR_SIZE;
	ulint end_offset
		= (data_len == OS_FILE_LOG_BLOCK_SIZE)
		? data_len - LOG_BLOCK_TRL_SIZE
		: data_len;
	ulint actual_data_len = (end_offset >= start_offset)
		? end_offset - start_offset : 0;

	ut_memcpy(log_bmp_sys->parse_buf_end, log_block + start_offset,
		  actual_data_len);

	log_bmp_sys->parse_buf_end += actual_data_len;

	ut_a(log_bmp_sys->parse_buf_end - log_bmp_sys->parse_buf
	     <= RECV_PARSING_BUF_SIZE);
}

/*********************************************************************//**
Parse the log block: first copies the read log data to the parse buffer while
skipping log block header, trailer and already parsed data.  Then it actually
parses the log to add to the modified page bitmap. */
static
void
log_online_parse_redo_log_block(
/*============================*/
	const byte*	log_block,		  /*!< in: read log data */
	ulint		skip_already_parsed_len)  /*!< in: how many bytes of
						  log data should be skipped as
						  they were parsed before */
{
	ulint block_data_len;

	block_data_len = log_block_get_data_len(log_block);

	ut_ad(block_data_len % OS_FILE_LOG_BLOCK_SIZE == 0
	      || block_data_len < OS_FILE_LOG_BLOCK_SIZE);

	log_online_add_to_parse_buf(log_block, block_data_len,
				    skip_already_parsed_len);
	log_online_parse_redo_log();
}

/*********************************************************************//**
Read and parse one redo log chunk and updates the modified page bitmap. */
static
void
log_online_follow_log_seg(
/*======================*/
	log_group_t*	group,		       /*!< in: the log group to use */
	ib_uint64_t	block_start_lsn,       /*!< in: the LSN to read from */
	ib_uint64_t	block_end_lsn)	       /*!< in: the LSN to read to */
{
	/* Pointer to the current OS_FILE_LOG_BLOCK-sized chunk of the read log
	data to parse */
	byte* log_block = log_bmp_sys->read_buf;
	byte* log_block_end = log_bmp_sys->read_buf
		+ (block_end_lsn - block_start_lsn);

	mutex_enter(&log_sys->mutex);
	log_group_read_log_seg(LOG_RECOVER, log_bmp_sys->read_buf,
			       group, block_start_lsn, block_end_lsn);
	mutex_exit(&log_sys->mutex);

	while (log_block < log_block_end
	       && log_bmp_sys->next_parse_lsn < log_bmp_sys->end_lsn) {

		/* How many bytes of log data should we skip in the current log
		block.  Skipping is necessary because we round down the next
		parse LSN thus it is possible to read the already-processed log
		data many times */
		ulint skip_already_parsed_len = 0;

		if (!log_online_is_valid_log_seg(log_block)) {
			break;
		}

		if ((block_start_lsn <= log_bmp_sys->next_parse_lsn)
		    && (block_start_lsn + OS_FILE_LOG_BLOCK_SIZE
			> log_bmp_sys->next_parse_lsn)) {

			/* The next parse LSN is inside the current block, skip
			data preceding it. */
			skip_already_parsed_len
				= log_bmp_sys->next_parse_lsn
				- block_start_lsn;
		}
		else {

			/* If the next parse LSN is not inside the current
			block, then the only option is that we have processed
			ahead already. */
			ut_a(block_start_lsn > log_bmp_sys->next_parse_lsn);
		}

		/* TODO: merge the copying to the parse buf code with
		skip_already_len calculations */
		log_online_parse_redo_log_block(log_block,
						skip_already_parsed_len);

		log_block += OS_FILE_LOG_BLOCK_SIZE;
		block_start_lsn += OS_FILE_LOG_BLOCK_SIZE;
	}

	return;
}

/*********************************************************************//**
Read and parse the redo log in a given group in FOLLOW_SCAN_SIZE-sized
chunks and updates the modified page bitmap. */
static
void
log_online_follow_log_group(
/*========================*/
	log_group_t*	group,		/*!< in: the log group to use */
	ib_uint64_t	contiguous_lsn)	/*!< in: the LSN of log block start
					containing the log_parse_start_lsn */
{
	ib_uint64_t block_start_lsn = contiguous_lsn;
	ib_uint64_t block_end_lsn;

	log_bmp_sys->next_parse_lsn = log_bmp_sys->start_lsn;
	log_bmp_sys->parse_buf_end = log_bmp_sys->parse_buf;

	do {
		block_end_lsn = block_start_lsn + FOLLOW_SCAN_SIZE;

		log_online_follow_log_seg(group, block_start_lsn,
					  block_end_lsn);

		/* Next parse LSN can become higher than the last read LSN
		only in the case when the read LSN falls right on the block
		boundary, in which case next parse lsn is bumped to the actual
		data LSN on the next (not yet read) block.  This assert is
		slightly conservative.  */
		ut_a(log_bmp_sys->next_parse_lsn
		     <= block_end_lsn + LOG_BLOCK_HDR_SIZE
		     + LOG_BLOCK_TRL_SIZE);

		block_start_lsn = block_end_lsn;
	} while (block_end_lsn < log_bmp_sys->end_lsn);

	/* Assert that the last read log record is a full one */
	ut_a(log_bmp_sys->parse_buf_end == log_bmp_sys->parse_buf);
}

/*********************************************************************//**
Write, flush one bitmap block to disk and advance the output position if
successful. */
static
void
log_online_write_bitmap_page(
/*=========================*/
	const byte *block)	/*!< in: block to write */
{
	ibool	success;

	success = os_file_write(log_bmp_sys->out_name,log_bmp_sys->out,
				block,
				(ulint)(log_bmp_sys->out_offset & 0xFFFFFFFF),
				(ulint)(log_bmp_sys->out_offset << 32),
				MODIFIED_PAGE_BLOCK_SIZE);
	if (UNIV_UNLIKELY(!success)) {

		/* The following call prints an error message */
		os_file_get_last_error(TRUE);
		fprintf(stderr, "InnoDB: Error: failed writing changed page "
			"bitmap file \'%s\'\n", log_bmp_sys->out_name);
		return;
	}

	success = os_file_flush(log_bmp_sys->out, FALSE);
	if (UNIV_UNLIKELY(!success)) {

		/* The following call prints an error message */
		os_file_get_last_error(TRUE);
		fprintf(stderr, "InnoDB: Error: failed flushing "
			"changed page bitmap file \'%s\'\n",
			log_bmp_sys->out_name);
		return;
	}

	log_bmp_sys->out_offset += MODIFIED_PAGE_BLOCK_SIZE;
}

/*********************************************************************//**
Append the current changed page bitmap to the bitmap file.  Clears the
bitmap tree and recycles its nodes to the free list. */
static
void
log_online_write_bitmap()
/*=====================*/
{
	ib_rbt_node_t		*bmp_tree_node;
	const ib_rbt_node_t	*last_bmp_tree_node;

	bmp_tree_node = (ib_rbt_node_t *)
		rbt_first(log_bmp_sys->modified_pages);
	last_bmp_tree_node = rbt_last(log_bmp_sys->modified_pages);

	while (bmp_tree_node) {

		byte *page = rbt_value(byte, bmp_tree_node);

		if (bmp_tree_node == last_bmp_tree_node) {
			mach_write_to_4(page + MODIFIED_PAGE_IS_LAST_BLOCK, 1);
		}

		mach_write_ull(page + MODIFIED_PAGE_START_LSN,
			       log_bmp_sys->start_lsn);
		mach_write_ull(page + MODIFIED_PAGE_END_LSN,
			       log_bmp_sys->end_lsn);
		mach_write_to_4(page + MODIFIED_PAGE_BLOCK_CHECKSUM,
				log_online_calc_checksum(page));

		log_online_write_bitmap_page(page);

		bmp_tree_node->left = log_bmp_sys->page_free_list;
		log_bmp_sys->page_free_list = bmp_tree_node;

		bmp_tree_node = (ib_rbt_node_t*)
			rbt_next(log_bmp_sys->modified_pages, bmp_tree_node);
	}

	rbt_reset(log_bmp_sys->modified_pages);
}

/*********************************************************************//**
Read and parse the redo log up to last checkpoint LSN to build the changed
page bitmap which is then written to disk.  */
UNIV_INTERN
void
log_online_follow_redo_log()
/*========================*/
{
	ib_uint64_t	contiguous_start_lsn;
	log_group_t*	group;

	/* Grab the LSN of the last checkpoint, we will parse up to it */
	mutex_enter(&(log_sys->mutex));
	log_bmp_sys->end_lsn = log_sys->last_checkpoint_lsn;
	mutex_exit(&(log_sys->mutex));

	if (log_bmp_sys->end_lsn == log_bmp_sys->start_lsn) {
		return;
	}

	group = UT_LIST_GET_FIRST(log_sys->log_groups);
	ut_a(group);

	contiguous_start_lsn = ut_uint64_align_down(log_bmp_sys->start_lsn,
						    OS_FILE_LOG_BLOCK_SIZE);

	while (group) {
		log_online_follow_log_group(group, contiguous_start_lsn);
		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	/* A crash injection site that ensures last checkpoint LSN > last
	tracked LSN, so that LSN tracking for this interval is tested. */
	DBUG_EXECUTE_IF("crash_before_bitmap_write", DBUG_SUICIDE(););

	log_online_write_bitmap();
	log_bmp_sys->start_lsn = log_bmp_sys->end_lsn;
	log_set_tracked_lsn(log_bmp_sys->start_lsn);
}

/*********************************************************************//**
Initializes log bitmap iterator.
@return TRUE if the iterator is initialized OK, FALSE otherwise. */
UNIV_INTERN
ibool
log_online_bitmap_iterator_init(
/*============================*/
	log_bitmap_iterator_t *i) /*!<in/out:  iterator */
{
	ibool	success;

	ut_a(i);
	ut_snprintf(i->in_name, FN_REFLEN, "%s%s%d", srv_data_home,
		    modified_page_stem, 1);
	i->in_offset = 0;
	/*
	  Set up bit offset out of the reasonable limit
	  to intiate reading block from file in
	  log_online_bitmap_iterator_next()
	*/
	i->bit_offset = MODIFIED_PAGE_BLOCK_BITMAP_LEN;
	i->in =
		os_file_create_simple_no_error_handling(
							i->in_name,
							OS_FILE_OPEN,
							OS_FILE_READ_ONLY,
							&success);

	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);
		fprintf(stderr,
			"InnoDB: Error: Cannot open \'%s\'\n",
			i->in_name);
		return FALSE;
	}

	i->page = ut_malloc(MODIFIED_PAGE_BLOCK_SIZE);

	i->start_lsn = i->end_lsn = 0;
	i->space_id = 0;
	i->first_page_id = 0;
	i->changed = FALSE;

	return TRUE;
}

/*********************************************************************//**
Releases log bitmap iterator. */
UNIV_INTERN
void
log_online_bitmap_iterator_release(
/*===============================*/
	log_bitmap_iterator_t *i) /*!<in/out:  iterator */
{
	ut_a(i);
	os_file_close(i->in);
	ut_free(i->page);
}

/*********************************************************************//**
Iterates through bits of saved bitmap blocks.
Sequentially reads blocks from bitmap file(s) and interates through
their bits. Ignores blocks with wrong checksum.
@return TRUE if iteration is successful, FALSE if all bits are iterated. */
UNIV_INTERN
ibool
log_online_bitmap_iterator_next(
/*============================*/
	log_bitmap_iterator_t *i) /*!<in/out: iterator */
{
	ulint	offset_low;
	ulint	offset_high;
	ulint	size_low;
	ulint	size_high;
	ulint	checksum	= 0;
	ulint	actual_checksum	= !checksum;

	ibool	success;

	ut_a(i);

	if (i->bit_offset < MODIFIED_PAGE_BLOCK_BITMAP_LEN)
	{
		++i->bit_offset;
		i->changed =
			IS_BIT_SET(i->page + MODIFIED_PAGE_BLOCK_BITMAP,
				   i->bit_offset);
		return TRUE;
	}

	while (checksum != actual_checksum)
	{
		success = os_file_get_size(i->in,
					   &size_low,
					   &size_high);
		if (!success) {
			os_file_get_last_error(TRUE);
			fprintf(stderr,
				"InnoDB: Warning: can't get size of "
				"page bitmap file \'%s\'\n",
				i->in_name);
			return FALSE;
		}

		if (i->in_offset >=
		    (ib_uint64_t)(size_low) +
		    ((ib_uint64_t)(size_high) << 32))
			return FALSE;

		offset_high = (ulint)(i->in_offset >> 32);
		offset_low = (ulint)(i->in_offset & 0xFFFFFFFF);

		success = os_file_read(
				       i->in,
				       i->page,
				       offset_low,
				       offset_high,
				       MODIFIED_PAGE_BLOCK_SIZE);

		if (!success) {
			os_file_get_last_error(TRUE);
			fprintf(stderr,
				"InnoDB: Warning: failed reading "
				"changed page bitmap file \'%s\'\n",
				i->in_name);
			return FALSE;
		}

		checksum = mach_read_from_4(
			i->page + MODIFIED_PAGE_BLOCK_CHECKSUM);

		actual_checksum = log_online_calc_checksum(i->page);

		i->in_offset += MODIFIED_PAGE_BLOCK_SIZE;
	}

	i->start_lsn =
		mach_read_ull(i->page + MODIFIED_PAGE_START_LSN);
	i->end_lsn =
		mach_read_ull(i->page + MODIFIED_PAGE_END_LSN);
	i->space_id =
		mach_read_from_4(i->page + MODIFIED_PAGE_SPACE_ID);
	i->first_page_id =
		mach_read_from_4(i->page + MODIFIED_PAGE_1ST_PAGE_ID);
	i->bit_offset =
		0;
	i->changed  =
		IS_BIT_SET(i->page + MODIFIED_PAGE_BLOCK_BITMAP,
			   i->bit_offset);

	return TRUE;
}

