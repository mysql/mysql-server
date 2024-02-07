/*****************************************************************************

Copyright (c) 2005, 2024, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/page0zip.h
 Compressed page interface

 Created June 2005 by Marko Makela
 *******************************************************/

#ifndef page0zip_h
#define page0zip_h

#include <sys/types.h>
#include <zlib.h>

#include "buf0buf.h"
#include "buf0checksum.h"
#include "buf0types.h"
#include "data0type.h"
#include "dict0types.h"
#include "mach0data.h"
#include "mem0mem.h"
#include "mtr0types.h"
#include "page/zipdecompress.h"
#include "page0types.h"
#include "srv0srv.h"
#include "trx0types.h"
#include "univ.i"
#include "ut0crc32.h"

/* Compression level to be used by zlib. Settable by user. */
extern uint page_zip_level;

/* Default compression level. */
constexpr uint32_t DEFAULT_COMPRESSION_LEVEL = 6;
/** Start offset of the area that will be compressed */
#define PAGE_ZIP_START PAGE_NEW_SUPREMUM_END
/** Predefine the sum of DIR_SLOT, TRX_ID & ROLL_PTR */
constexpr uint32_t PAGE_ZIP_CLUST_LEAF_SLOT_SIZE =
    PAGE_ZIP_DIR_SLOT_SIZE + DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN;
/** Mask of record offsets */
constexpr uint32_t PAGE_ZIP_DIR_SLOT_MASK = 0x3fff;
/** 'owned' flag */
constexpr uint32_t PAGE_ZIP_DIR_SLOT_OWNED = 0x4000;
/** 'deleted' flag */
constexpr uint32_t PAGE_ZIP_DIR_SLOT_DEL = 0x8000;

/* Whether or not to log compressed page images to avoid possible
compression algorithm changes in zlib. */
extern bool page_zip_log_pages;

/** Set the size of a compressed page in bytes.
@param[in,out]  page_zip        compressed page
@param[in]      size            size in bytes */
static inline void page_zip_set_size(page_zip_des_t *page_zip, ulint size);

#ifndef UNIV_HOTBACKUP
/** Determine if a record is so big that it needs to be stored externally.
@param[in]      rec_size        length of the record in bytes
@param[in]      comp            nonzero=compact format
@param[in]      n_fields        number of fields in the record; ignored if
tablespace is not compressed
@param[in]      page_size       page size
@return false if the entire record can be stored locally on the page */
[[nodiscard]] static inline bool page_zip_rec_needs_ext(
    ulint rec_size, ulint comp, ulint n_fields, const page_size_t &page_size);
#endif /* !UNIV_HOTBACKUP */

/** Determine the guaranteed free space on an empty page.
@param[in]  n_fields  number of columns in the index
@param[in]  zip_size  compressed page size in bytes
@return minimum payload size on the page */
ulint page_zip_empty_size(ulint n_fields, ulint zip_size);

#ifndef UNIV_HOTBACKUP
/** Check whether a tuple is too big for compressed table
@param[in]      index   dict index object
@param[in]      entry   entry for the index
@return true if it's too big, otherwise false */
bool page_zip_is_too_big(const dict_index_t *index, const dtuple_t *entry);
#endif /* !UNIV_HOTBACKUP */

/** Initialize a compressed page descriptor. */
static inline void page_zip_des_init(
    page_zip_des_t *page_zip); /*!< in/out: compressed page
                               descriptor */

/** Configure the zlib allocator to use the given memory heap.
@param[in,out] stream zlib stream
@param[in] heap Memory heap to use */
void page_zip_set_alloc(void *stream, mem_heap_t *heap);

/** Compress a page.
 @return true on success, false on failure; page_zip will be left
 intact on failure. */
bool page_zip_compress(page_zip_des_t *page_zip, /*!< in: size; out: data,
                                                  n_blobs, m_start, m_end,
                                                  m_nonempty */
                       const page_t *page,       /*!< in: uncompressed page */
                       dict_index_t *index,      /*!< in: index tree */
                       ulint level,              /*!< in: compression level */
                       mtr_t *mtr);              /*!< in/out: mini-transaction,
                                                 or NULL */

/** Write the index information for the compressed page.
 @return used size of buf */
ulint page_zip_fields_encode(
    ulint n,                   /*!< in: number of fields
                               to compress */
    const dict_index_t *index, /*!< in: index comprising
                               at least n fields */
    ulint trx_id_pos,
    /*!< in: position of the trx_id column
    in the index, or ULINT_UNDEFINED if
    this is a non-leaf page */
    byte *buf); /*!< out: buffer of (n + 1) * 2 bytes */

/** Decompress a page.  This function should tolerate errors on the compressed
 page.  Instead of letting assertions fail, it will return false if an
 inconsistency is detected.
 @return true on success, false on failure */
bool page_zip_decompress(
    page_zip_des_t *page_zip, /*!< in: data, ssize;
                             out: m_start, m_end, m_nonempty, n_blobs */
    page_t *page,             /*!< out: uncompressed page, may be trashed */
    bool all);                /*!< in: true=decompress the whole page;
                               false=verify but do not copy some
                               page header fields that should not change
                               after page creation */

#ifdef UNIV_ZIP_DEBUG
/** Check that the compressed and decompressed pages match.
 @return true if valid, false if not */
bool page_zip_validate_low(
    const page_zip_des_t *page_zip, /*!< in: compressed page */
    const page_t *page,             /*!< in: uncompressed page */
    const dict_index_t *index,      /*!< in: index of the page, if known */
    bool sloppy);                   /*!< in: false=strict,
                             true=ignore the MIN_REC_FLAG */
/** Check that the compressed and decompressed pages match. */
bool page_zip_validate(
    const page_zip_des_t *page_zip, /*!< in: compressed page */
    const page_t *page,             /*!< in: uncompressed page */
    const dict_index_t *index);     /*!< in: index of the page, if known */
#endif                              /* UNIV_ZIP_DEBUG */

/** Determine how big record can be inserted without re-compressing the page.
@param[in] page_zip Compressed page.
@param[in] is_clust True if clustered index.
@return a positive number indicating the maximum size of a record
whose insertion is guaranteed to succeed, or zero or negative */
[[nodiscard]] static inline lint page_zip_max_ins_size(
    const page_zip_des_t *page_zip, bool is_clust);

/** Determine if enough space is available in the modification log.
@param[in] page_zip Compressed page.
@param[in] is_clust True if clustered index.
@param[in] length Combined size of the record.
@param[in] create Nonzero=add the record to the heap.
@return true if page_zip_write_rec() will succeed */
[[nodiscard]] static inline bool page_zip_available(
    const page_zip_des_t *page_zip, bool is_clust, ulint length, ulint create);

/** Write data to the uncompressed header portion of a page.  The data must
already have been written to the uncompressed page.
However, the data portion of the uncompressed page may differ from the
compressed page when a record is being inserted in page_cur_insert_rec_low().
@param[in,out]  page_zip        Compressed page
@param[in]      str             Address on the uncompressed page
@param[in]      length          Length of the data
@param[in]      mtr             Mini-transaction, or NULL */
static inline void page_zip_write_header(page_zip_des_t *page_zip,
                                         const byte *str, ulint length,
                                         mtr_t *mtr);

/** Write an entire record on the compressed page.  The data must already
 have been written to the uncompressed page. */
void page_zip_write_rec(
    page_zip_des_t *page_zip,  /*!< in/out: compressed page */
    const byte *rec,           /*!< in: record being written */
    const dict_index_t *index, /*!< in: the index the record belongs to */
    const ulint *offsets,      /*!< in: rec_get_offsets(rec, index) */
    ulint create);             /*!< in: nonzero=insert, zero=update */

/** Parses a log record of writing a BLOB pointer of a record.
 @return end of log record or NULL */
const byte *page_zip_parse_write_blob_ptr(
    const byte *ptr,           /*!< in: redo log buffer */
    const byte *end_ptr,       /*!< in: redo log buffer end */
    page_t *page,              /*!< in/out: uncompressed page */
    page_zip_des_t *page_zip); /*!< in/out: compressed page */

/** Write a BLOB pointer of a record on the leaf page of a clustered index.
 The information must already have been updated on the uncompressed page. */
void page_zip_write_blob_ptr(
    page_zip_des_t *page_zip,  /*!< in/out: compressed page */
    const byte *rec,           /*!< in/out: record whose data is being
                               written */
    const dict_index_t *index, /*!< in: index of the page */
    const ulint *offsets,      /*!< in: rec_get_offsets(rec, index) */
    ulint n,                   /*!< in: column index */
    mtr_t *mtr);               /*!< in: mini-transaction handle,
                       or NULL if no logging is needed */

/** Parses a log record of writing the node pointer of a record.
 @return end of log record or NULL */
const byte *page_zip_parse_write_node_ptr(
    const byte *ptr,           /*!< in: redo log buffer */
    const byte *end_ptr,       /*!< in: redo log buffer end */
    page_t *page,              /*!< in/out: uncompressed page */
    page_zip_des_t *page_zip); /*!< in/out: compressed page */

/** Write the node pointer of a record on a non-leaf compressed page.
@param[in,out] page_zip Compressed page
@param[in,out] rec Record
@param[in] size Data size of rec
@param[in] ptr Node pointer
@param[in] mtr Mini-transaction, or null */
void page_zip_write_node_ptr(page_zip_des_t *page_zip, byte *rec, ulint size,
                             ulint ptr, mtr_t *mtr);

/** Write the trx_id and roll_ptr of a record on a B-tree leaf node page.
@param[in,out] page_zip Compressed page
@param[in,out] rec Record
@param[in] offsets Rec_get_offsets(rec, index)
@param[in] trx_id_col Column number of trx_id in rec
@param[in] trx_id Transaction identifier
@param[in] roll_ptr Roll_ptr */
void page_zip_write_trx_id_and_roll_ptr(page_zip_des_t *page_zip, byte *rec,
                                        const ulint *offsets, ulint trx_id_col,
                                        trx_id_t trx_id, roll_ptr_t roll_ptr);

/** Write the "deleted" flag of a record on a compressed page.  The flag must
 already have been written on the uncompressed page. */
void page_zip_rec_set_deleted(
    page_zip_des_t *page_zip, /*!< in/out: compressed page */
    const byte *rec,          /*!< in: record on the uncompressed page */
    bool flag);               /*!< in: the deleted flag (nonzero=true) */

/** Write the "owned" flag of a record on a compressed page. The n_owned field
must already have been written on the uncompressed page.
@param[in,out]  page_zip  Compressed page
@param[in]      rec       Record on the uncompressed page
@param[in]      flag      The owned flag (nonzero=true) */
void page_zip_rec_set_owned(page_zip_des_t *page_zip, const byte *rec,
                            ulint flag);

/** Insert a record to the dense page directory.
@param[in,out] page_zip Compressed page
@param[in] prev_rec Record after which to insert
@param[in] free_rec Record from which rec was allocated, or null
@param[in] rec Record to insert */
void page_zip_dir_insert(page_zip_des_t *page_zip, const byte *prev_rec,
                         const byte *free_rec, byte *rec);

/** Shift the dense page directory and the array of BLOB pointers when a record
is deleted.
@param[in,out]  page_zip        compressed page
@param[in]      rec             deleted record
@param[in]      index           index of rec
@param[in]      offsets         rec_get_offsets(rec)
@param[in]      free            previous start of the free list */
void page_zip_dir_delete(page_zip_des_t *page_zip, byte *rec,
                         const dict_index_t *index, const ulint *offsets,
                         const byte *free);

/** Add a slot to the dense page directory.
@param[in,out] page_zip Compressed page
@param[in] is_clustered Nonzero for clustered index, zero for others */
void page_zip_dir_add_slot(page_zip_des_t *page_zip, bool is_clustered);

/** Parses a log record of writing to the header of a page.
 @return end of log record or NULL */
const byte *page_zip_parse_write_header(
    const byte *ptr,           /*!< in: redo log buffer */
    const byte *end_ptr,       /*!< in: redo log buffer end */
    page_t *page,              /*!< in/out: uncompressed page */
    page_zip_des_t *page_zip); /*!< in/out: compressed page */

/** Reorganize and compress a page.  This is a low-level operation for
 compressed pages, to be used when page_zip_compress() fails.
 On success, a redo log entry MLOG_ZIP_PAGE_COMPRESS will be written.
 The function btr_page_reorganize() should be preferred whenever possible.
 IMPORTANT: if page_zip_reorganize() is invoked on a leaf page of a
 non-clustered index, the caller must update the insert buffer free
 bits in the same mini-transaction in such a way that the modification
 will be redo-logged.
 @return true on success, false on failure; page_zip will be left
 intact on failure, but page will be overwritten. */
bool page_zip_reorganize(
    buf_block_t *block,  /*!< in/out: page with compressed page;
                         on the compressed page, in: size;
                         out: data, n_blobs,
                         m_start, m_end, m_nonempty */
    dict_index_t *index, /*!< in: index of the B-tree node */
    mtr_t *mtr);         /*!< in: mini-transaction */
/** Copy the records of a page byte for byte.  Do not copy the page header
 or trailer, except those B-tree header fields that are directly
 related to the storage of records.  Also copy PAGE_MAX_TRX_ID.
 NOTE: The caller must update the lock table and the adaptive hash index. */
void page_zip_copy_recs(
    page_zip_des_t *page_zip,      /*!< out: copy of src_zip
                                   (n_blobs, m_start, m_end,
                                   m_nonempty, data[0..size-1]) */
    page_t *page,                  /*!< out: copy of src */
    const page_zip_des_t *src_zip, /*!< in: compressed page */
    const page_t *src,             /*!< in: page */
    dict_index_t *index,           /*!< in: index of the B-tree */
    mtr_t *mtr);                   /*!< in: mini-transaction */
#ifndef UNIV_HOTBACKUP
#endif /* !UNIV_HOTBACKUP */

/** Parses a log record of compressing an index page.
 @return end of log record or NULL */
const byte *page_zip_parse_compress(
    const byte *ptr,           /*!< in: buffer */
    const byte *end_ptr,       /*!< in: buffer end */
    page_t *page,              /*!< out: uncompressed page */
    page_zip_des_t *page_zip); /*!< out: compressed page */

/** Write a log record of compressing an index page without the data on the
page.
@param[in]      level   compression level
@param[in]      page    page that is compressed
@param[in]      index   index
@param[in]      mtr     mtr */
static inline void page_zip_compress_write_log_no_data(ulint level,
                                                       const page_t *page,
                                                       dict_index_t *index,
                                                       mtr_t *mtr);

/** Parses a log record of compressing an index page without the data.
@param[in]      ptr             buffer
@param[in]      end_ptr         buffer end
@param[in]      page            uncompressed page
@param[out]     page_zip        compressed page
@param[in]      index           index
@return end of log record or NULL */
static inline const byte *page_zip_parse_compress_no_data(
    const byte *ptr, const byte *end_ptr, page_t *page,
    page_zip_des_t *page_zip, dict_index_t *index);

#ifndef UNIV_HOTBACKUP
/** Reset the counters used for filling
 INFORMATION_SCHEMA.innodb_cmp_per_index. */
static inline void page_zip_reset_stat_per_index();

#endif /* !UNIV_HOTBACKUP */

#include "page0zip.ic"

#endif /* page0zip_h */
