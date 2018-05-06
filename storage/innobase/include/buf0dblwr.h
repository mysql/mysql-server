/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/buf0dblwr.h
 Doublewrite buffer module

 Created 2011/12/19 Inaam Rana
 *******************************************************/

#ifndef buf0dblwr_h
#define buf0dblwr_h

#include "buf0types.h"
#include "log0log.h"
#include "log0recv.h"
#include "univ.i"
#include "ut0byte.h"

/** Doublewrite system */
extern buf_dblwr_t *buf_dblwr;
/** Set to TRUE when the doublewrite buffer is being created */
extern ibool buf_dblwr_being_created;

/** Creates the doublewrite buffer to a new InnoDB installation. The header of
 the doublewrite buffer is placed on the trx system header page.
 @return true if successful, false if not. */
MY_ATTRIBUTE((warn_unused_result))
bool buf_dblwr_create(void);

/** At a database startup initializes the doublewrite buffer memory structure if
 we already have a doublewrite buffer created in the data files. If we are
 upgrading to an InnoDB version which supports multiple tablespaces, then this
 function performs the necessary update operations. If we are in a crash
 recovery, this function loads the pages from double write buffer into memory.
 @return DB_SUCCESS or error code */
dberr_t buf_dblwr_init_or_load_pages(pfs_os_file_t file, const char *path);

/** Process and remove the double write buffer pages for all tablespaces. */
void buf_dblwr_process(void);

/** frees doublewrite buffer. */
void buf_dblwr_free(void);
/** Updates the doublewrite buffer when an IO request is completed. */
void buf_dblwr_update(
    const buf_page_t *bpage, /*!< in: buffer block descriptor */
    buf_flush_t flush_type); /*!< in: flush type */
/** Determines if a page number is located inside the doublewrite buffer.
 @return true if the location is inside the two blocks of the
 doublewrite buffer */
ibool buf_dblwr_page_inside(page_no_t page_no); /*!< in: page number */

/** Posts a buffer page for writing. If the doublewrite memory buffer
is full, calls buf_dblwr_flush_buffered_writes and waits for for free
space to appear.
@param[in]	bpage	buffer block to write */
void buf_dblwr_add_to_batch(buf_page_t *bpage);

/** Flush a batch of writes to the datafiles that have already been
 written to the dblwr buffer on disk. */
void buf_dblwr_sync_datafiles();

/** Flushes possible buffered writes from the doublewrite memory buffer to disk,
 and also wakes up the aio thread if simulated aio is used. It is very
 important to call this function after a batch of writes has been posted,
 and also when we may have to wait for a page latch! Otherwise a deadlock
 of threads can occur. */
void buf_dblwr_flush_buffered_writes(void);
/** Writes a page to the doublewrite buffer on disk, sync it, then write
 the page to the datafile and sync the datafile. This function is used
 for single page flushes. If all the buffers allocated for single page
 flushes in the doublewrite buffer are in use we wait here for one to
 become free. We are guaranteed that a slot will become free because any
 thread that is using a slot must also release the slot before leaving
 this function. */
void buf_dblwr_write_single_page(
    buf_page_t *bpage, /*!< in: buffer block to write */
    bool sync);        /*!< in: true if sync IO requested */

/** Recover pages from the double write buffer for a specific tablespace.
The pages that were read from the doublewrite buffer are written to the
tablespace they belong to.
@param[in]	space		Tablespace instance */
void buf_dblwr_recover_pages(fil_space_t *space);

/** Doublewrite control struct */
struct buf_dblwr_t {
  ib_mutex_t mutex;           /*!< mutex protecting the first_free
                              field and write_buf */
  page_no_t block1;           /*!< the page number of the first
                              doublewrite block (64 pages) */
  page_no_t block2;           /*!< page number of the second block */
  page_no_t first_free;       /*!< first free position in write_buf
                           measured in units of UNIV_PAGE_SIZE */
  ulint b_reserved;           /*!< number of slots currently reserved
                           for batch flush. */
  os_event_t b_event;         /*!< event where threads wait for a
                              batch flush to end. */
  ulint s_reserved;           /*!< number of slots currently
                           reserved for single page flushes. */
  os_event_t s_event;         /*!< event where threads wait for a
                              single page flush slot. */
  bool *in_use;               /*!< flag used to indicate if a slot is
                              in use. Only used for single page
                              flushes. */
  bool batch_running;         /*!< set to TRUE if currently a batch
                        is being written from the doublewrite
                        buffer. */
  byte *write_buf;            /*!< write buffer used in writing to the
                            doublewrite buffer, aligned to an
                            address divisible by UNIV_PAGE_SIZE
                            (which is required by Windows aio) */
  byte *write_buf_unaligned;  /*!< pointer to write_buf,
                  but unaligned */
  buf_page_t **buf_block_arr; /*!< array to store pointers to
                        the buffer blocks which have been
                        cached to write_buf */
};

#endif
