/*****************************************************************************

Copyright (c) 2011-2012, Percona Inc. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
Street, Fifth Floor, Boston, MA 02110-1301, USA

*****************************************************************************/

/**************************************************//**
@file include/log0online.h
Online database log parsing for changed page tracking
*******************************************************/

#ifndef log0online_h
#define log0online_h

#include "univ.i"
#include "os0file.h"

/** Single bitmap file information */
typedef struct log_online_bitmap_file_struct log_online_bitmap_file_t;

/** A set of bitmap files containing some LSN range */
typedef struct log_online_bitmap_file_range_struct
log_online_bitmap_file_range_t;

/** An iterator over changed page info */
typedef struct log_bitmap_iterator_struct log_bitmap_iterator_t;

/*********************************************************************//**
Initializes the online log following subsytem. */
UNIV_INTERN
void
log_online_read_init(void);
/*=======================*/

/*********************************************************************//**
Shuts down the online log following subsystem. */
UNIV_INTERN
void
log_online_read_shutdown(void);
/*===========================*/

/*********************************************************************//**
Reads and parses the redo log up to last checkpoint LSN to build the changed
page bitmap which is then written to disk.

@return TRUE if log tracking succeeded, FALSE if bitmap write I/O error */
UNIV_INTERN
ibool
log_online_follow_redo_log(void);
/*=============================*/

/************************************************************//**
Delete all the bitmap files for data less than the specified LSN.
If called with lsn == 0 (i.e. set by RESET request) or
IB_ULONGLONG_MAX, restart the bitmap file sequence, otherwise
continue it.

@return FALSE to indicate success, TRUE for failure. */
UNIV_INTERN
ibool
log_online_purge_changed_page_bitmaps(
/*==================================*/
	ib_uint64_t lsn);	/*!<in: LSN to purge files up to */

/************************************************************//**
Delete all the bitmap files for data less than the specified LSN.
If called with lsn == 0 (i.e. set by RESET request) or
IB_ULONGLONG_MAX, restart the bitmap file sequence, otherwise
continue it.

@return FALSE to indicate success, TRUE for failure. */
UNIV_INTERN
ibool
log_online_purge_changed_page_bitmaps(
/*==================================*/
	ib_uint64_t lsn);	/*!<in: LSN to purge files up to */

#define LOG_BITMAP_ITERATOR_START_LSN(i) \
	((i).start_lsn)
#define LOG_BITMAP_ITERATOR_END_LSN(i) \
	((i).end_lsn)
#define LOG_BITMAP_ITERATOR_SPACE_ID(i) \
	((i).space_id)
#define LOG_BITMAP_ITERATOR_PAGE_NUM(i) \
	((i).first_page_id + (i).bit_offset)
#define LOG_BITMAP_ITERATOR_PAGE_CHANGED(i) \
	((i).changed)

/*********************************************************************//**
Initializes log bitmap iterator.  The minimum LSN is used for finding the
correct starting file with records and it there may be records returned by
the iterator that have LSN less than start_lsn.

@return TRUE if the iterator is initialized OK, FALSE otherwise. */
UNIV_INTERN
ibool
log_online_bitmap_iterator_init(
/*============================*/
	log_bitmap_iterator_t	*i,		/*!<in/out:  iterator */
	ib_uint64_t		min_lsn,	/*!<in: start LSN for the
						iterator */
	ib_uint64_t		max_lsn);	/*!<in: end LSN for the
						iterator */

/*********************************************************************//**
Releases log bitmap iterator. */
UNIV_INTERN
void
log_online_bitmap_iterator_release(
/*===============================*/
	log_bitmap_iterator_t *i); /*!<in/out:  iterator */

/*********************************************************************//**
Iterates through bits of saved bitmap blocks.
Sequentially reads blocks from bitmap file(s) and interates through
their bits. Ignores blocks with wrong checksum.
@return TRUE if iteration is successful, FALSE if all bits are iterated. */
UNIV_INTERN
ibool
log_online_bitmap_iterator_next(
/*============================*/
	log_bitmap_iterator_t *i); /*!<in/out: iterator */

/** Struct for single bitmap file information */
struct log_online_bitmap_file_struct {
	char		name[FN_REFLEN];	/*!< Name with full path */
	os_file_t	file;			/*!< Handle to opened file */
	ib_uint64_t	size;			/*!< Size of the file */
	ib_uint64_t	offset;			/*!< Offset of the next read,
						or count of already-read bytes
						*/
};

/** Struct for a set of bitmap files containing some LSN range */
struct log_online_bitmap_file_range_struct {
	size_t	count;					/*!< Number of files */
	/*!< Dynamically-allocated array of info about individual files */
	struct {
		char		name[FN_REFLEN];	/*!< Name of a file */
		ib_uint64_t	start_lsn;		/*!< Starting LSN of
						        data in	this file */
		ulong		seq_num;		/*!< Sequence number of
							this file */
	}	*files;
};

/** Struct for an iterator through all bits of changed pages bitmap blocks */
struct log_bitmap_iterator_struct
{
	ibool				failed;		/*!< Has the iteration
							stopped prematurely */
	log_online_bitmap_file_range_t	in_files;	/*!< The bitmap files
							for this iterator */
	size_t				in_i;		/*!< Currently read
							file index in in_files
							*/
	log_online_bitmap_file_t	in;		/*!< Currently read
							file */
	ib_uint32_t			bit_offset;	/*!< bit offset inside
							the current bitmap
							block */
	ib_uint64_t			start_lsn;	/*!< Start LSN of the
							current bitmap block */
	ib_uint64_t			end_lsn;	/*!< End LSN of the
							current bitmap block */
	ib_uint32_t			space_id;	/*!< Current block
							space id */
	ib_uint32_t			first_page_id;	/*!< Id of the first
							page in the current
							block */
	ibool				last_page_in_run;/*!< "Last page in
							run" flag value for the
							current block */
	ibool				changed;	/*!< true if current
							page was changed */
	byte*				page;		/*!< Bitmap block */
};

#endif
