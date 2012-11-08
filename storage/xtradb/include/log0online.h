/*****************************************************************************

Copyright (c) 2011-2012, Percona Inc. All Rights Reserved.

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
@file include/log0online.h
Online database log parsing for changed page tracking
*******************************************************/

#ifndef log0online_h
#define log0online_h

#include "univ.i"
#include "os0file.h"

/*********************************************************************//**
Initializes the online log following subsytem. */
UNIV_INTERN
void
log_online_read_init();
/*===================*/

/*********************************************************************//**
Shuts down the online log following subsystem. */
UNIV_INTERN
void
log_online_read_shutdown();
/*=======================*/

/*********************************************************************//**
Reads and parses the redo log up to last checkpoint LSN to build the changed
page bitmap which is then written to disk.  */
UNIV_INTERN
void
log_online_follow_redo_log();
/*=========================*/

/** The iterator through all bits of changed pages bitmap blocks */
struct log_bitmap_iterator_struct
{
	char		in_name[FN_REFLEN]; /*!< the file name for bitmap
					    input */
	os_file_t	in;                 /*!< the bitmap input file */
	ib_uint64_t	in_offset;          /*!< the next write position in the
					    bitmap output file */
	ib_uint32_t	bit_offset;         /*!< bit offset inside of bitmap
					    block*/
	ib_uint64_t	start_lsn;          /*!< Start lsn of the block */
	ib_uint64_t	end_lsn;            /*!< End lsn of the block */
	ib_uint32_t	space_id;           /*!< Block space id */
	ib_uint32_t	first_page_id;      /*!< First block page id */
	ibool		changed;            /*!< true if current page was changed */
	byte*		page;               /*!< Bitmap block */
};

typedef struct log_bitmap_iterator_struct log_bitmap_iterator_t;

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
Initializes log bitmap iterator.
@return TRUE if the iterator is initialized OK, FALSE otherwise. */
UNIV_INTERN
ibool
log_online_bitmap_iterator_init(
/*============================*/
	log_bitmap_iterator_t *i); /*!<in/out:  iterator */

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

#endif
