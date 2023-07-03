/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.

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

/** @file include/fsp0fsp.h
 File space management

 Created 12/18/1995 Heikki Tuuri
 *******************************************************/

#ifndef fsp0fsp_h
#define fsp0fsp_h

#include "univ.i"

#include "fsp0space.h"
#include "fut0lst.h"
#include "mtr0mtr.h"
#include "mysql/components/services/bits/mysql_cond_bits.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "page0types.h"
#include "rem0types.h"
#include "ut0byte.h"

#include <random>
#include "fsp0types.h"

#ifdef UNIV_HOTBACKUP
#include "buf0buf.h"
#endif /* UNIV_HOTBACKUP */

class DDL_Record;
extern std::vector<DDL_Record *> ts_encrypt_ddl_records;
extern mysql_cond_t resume_encryption_cond;
extern mysql_mutex_t resume_encryption_cond_m;

/** @defgroup Tablespace Header Constants (moved from fsp0fsp.c)
@{ */
#ifdef UNIV_DEBUG
extern std::vector<space_id_t> flag_mismatch_spaces;
#endif

/** Offset of the space header within a file page */
constexpr uint32_t FSP_HEADER_OFFSET = FIL_PAGE_DATA;

/** The number of bytes required to store SDI root page number(4)
and SDI version(4) at Page 0 */
constexpr uint32_t FSP_SDI_HEADER_LEN = 8;

/* The data structures in files are defined just as byte strings in C */
typedef byte fsp_header_t;
typedef byte xdes_t;

#ifdef UNIV_DEBUG
/** Check if the state of extent descriptor is valid.
@param[in]      state   the extent descriptor state
@return true if state is valid, false otherwise */
bool xdes_state_is_valid(ulint state);
#endif /* UNIV_DEBUG */

#ifdef UNIV_DEBUG
struct xdes_mem_t {
  xdes_mem_t(const xdes_t *xdes) : m_xdes(xdes) {}

  const char *state_name() const;

  bool is_valid() const;
  const xdes_t *m_xdes;

  std::ostream &print(std::ostream &out) const;
};

inline std::ostream &operator<<(std::ostream &out, const xdes_mem_t &obj) {
  return (obj.print(out));
}

/** In-memory representation of the fsp_header_t file structure. */
struct fsp_header_mem_t {
  fsp_header_mem_t(const fsp_header_t *header, mtr_t *mtr);

  ulint m_space_id;
  ulint m_notused;
  ulint m_fsp_size;
  ulint m_free_limit;
  ulint m_flags;
  ulint m_fsp_frag_n_used;
  flst_bnode_t m_fsp_free;
  flst_bnode_t m_free_frag;
  flst_bnode_t m_full_frag;
  ib_id_t m_segid;
  flst_bnode_t m_inodes_full;
  flst_bnode_t m_inodes_free;

  std::ostream &print(std::ostream &out) const;
};

inline std::ostream &operator<<(std::ostream &out,
                                const fsp_header_mem_t &obj) {
  return (obj.print(out));
}
#endif /* UNIV_DEBUG */

/*                      SPACE HEADER
                        ============

File space header data structure: this data structure is contained in the
first page of a space. The space for this header is reserved in every extent
descriptor page, but used only in the first. */

/*-------------------------------------*/
/** space id */
constexpr uint32_t FSP_SPACE_ID = 0;
/** this field contained a value up to which we know that the modifications in
 the database have been flushed to the file space; not used now */
constexpr uint32_t FSP_NOT_USED = 4;
/** Current size of the space in pages */
constexpr uint32_t FSP_SIZE = 8;
/** Minimum page number for which the  free list has not been initialized: the
 pages >= this limit are, bydefinition, free; note that in a single-table
 tablespace where size < 64 pages, this number is 64, i.e.,we have initialized
 the space about the first extent, but have not physically allocated those
 pages to thefile */
constexpr uint32_t FSP_FREE_LIMIT = 12;
/** fsp_space_t.flags, similar to dict_table_t::flags */
constexpr uint32_t FSP_SPACE_FLAGS = 16;
/** number of used pages in the FSP_FREE_FRAG list */
constexpr uint32_t FSP_FRAG_N_USED = 20;
/** list of free extents */
constexpr uint32_t FSP_FREE = 24;
/** list of partially free extents not belonging to any segment */
constexpr uint32_t FSP_FREE_FRAG = 24 + FLST_BASE_NODE_SIZE;

/** list of full extents not belonging to any segment */
constexpr uint32_t FSP_FULL_FRAG = 24 + 2 * FLST_BASE_NODE_SIZE;

/** 8 bytes which give the first unused segment id */
constexpr uint32_t FSP_SEG_ID = 24 + 3 * FLST_BASE_NODE_SIZE;

/** list of pages containing segment headers, where all the segment inode slots
 are reserved */
constexpr uint32_t FSP_SEG_INODES_FULL = 32 + 3 * FLST_BASE_NODE_SIZE;

/** list of pages containing segment headers, where not all the segment header
 slots are reserved */
constexpr uint32_t FSP_SEG_INODES_FREE = 32 + 4 * FLST_BASE_NODE_SIZE;

/*-------------------------------------*/
/* File space header size */
constexpr uint32_t FSP_HEADER_SIZE = 32 + 5 * FLST_BASE_NODE_SIZE;

/** This many free extents are added to the free list from above FSP_FREE_LIMIT
 at a time */
constexpr uint32_t FSP_FREE_ADD = 4;

/** @} */

/* Maximum allowed value of AUTOEXTEND_SIZE attribute */
constexpr uint64_t FSP_MAX_AUTOEXTEND_SIZE = 4 * 1024 * 1024 * 1024ULL;

/** @defgroup File Segment Inode Constants (moved from fsp0fsp.c)
@{ */

/*                      FILE SEGMENT INODE
                        ==================

Segment inode which is created for each segment in a tablespace. NOTE: in
purge we assume that a segment having only one currently used page can be
freed in a few steps, so that the freeing cannot fill the file buffer with
bufferfixed file pages. */

typedef byte fseg_inode_t;

constexpr uint32_t FSEG_INODE_PAGE_NODE = FSEG_PAGE_DATA;
/* the list node for linking
segment inode pages */

constexpr uint32_t FSEG_ARR_OFFSET = FSEG_PAGE_DATA + FLST_NODE_SIZE;
/*-------------------------------------*/
/* 8 bytes of segment id: if this is 0,  it means that the header is unused */
constexpr uint32_t FSEG_ID = 0;
/** number of used segment pages in the FSEG_NOT_FULL list */
constexpr uint32_t FSEG_NOT_FULL_N_USED = 8;
/** list of free extents of this segment */
constexpr uint32_t FSEG_FREE = 12;
/** list of partially free extents */
constexpr uint32_t FSEG_NOT_FULL = 12 + FLST_BASE_NODE_SIZE;
/** list of full extents */
constexpr uint32_t FSEG_FULL = 12 + 2 * FLST_BASE_NODE_SIZE;
/** magic number used in debugging */
constexpr uint32_t FSEG_MAGIC_N = 12 + 3 * FLST_BASE_NODE_SIZE;
/** array of individual pages belonging to this segment in fsp fragment extent
 lists */
constexpr uint32_t FSEG_FRAG_ARR = 16 + 3 * FLST_BASE_NODE_SIZE;
/* number of slots in the array for the fragment pages */
#define FSEG_FRAG_ARR_N_SLOTS (FSP_EXTENT_SIZE / 2)
/** a fragment page slot contains its  page number within space, FIL_NULL means
 that the slot is not in use */
constexpr uint32_t FSEG_FRAG_SLOT_SIZE = 4;

/*-------------------------------------*/
#define FSEG_INODE_SIZE \
  (16 + 3 * FLST_BASE_NODE_SIZE + FSEG_FRAG_ARR_N_SLOTS * FSEG_FRAG_SLOT_SIZE)

static inline uint32_t FSP_SEG_INODES_PER_PAGE(page_size_t page_size) {
  return (page_size.physical() - FSEG_ARR_OFFSET - 10) / FSEG_INODE_SIZE;
}
/* Number of segment inodes which fit on a
single page */

constexpr uint32_t FSEG_MAGIC_N_VALUE = 97937874;

/** The segment_reserve_factor is the ratio x/y expressed in percentage,
where x is the number of free pages in the segment, and y is the total number
of pages in the segment.  The number of used pages in the segment is given by
(y-x).  The number of free pages in the segment (x) will be maintained such
that the actual segment_reserve_factor will be >= the requested
segment_reserve_factor, which is contained in this variable. */
extern double fseg_reserve_pct;

/* Various constants related to segment reserve factor */
constexpr double FSEG_RESERVE_PCT_DFLT = 12.50;
constexpr double FSEG_RESERVE_PCT_MIN = 0.03;
constexpr double FSEG_RESERVE_PCT_MAX = 40.00;

#define FSEG_FRAG_LIMIT FSEG_FRAG_ARR_N_SLOTS

/** If the reserved size of a segment is at least this many extents, we allow
extents to be put to the free list of the extent: at most
FSEG_FREE_LIST_MAX_LEN many */
constexpr uint32_t FSEG_FREE_LIST_LIMIT = 40;
constexpr uint32_t FSEG_FREE_LIST_MAX_LEN = 4;
/** @} */

/** @defgroup Extent Descriptor Constants (moved from fsp0fsp.c)
@{ */

/*                      EXTENT DESCRIPTOR
                        =================

File extent descriptor data structure: contains bits to tell which pages in
the extent are free and which contain old tuple version to clean. */

/*-------------------------------------*/
/** The identifier of the segment to which this extent belongs */
constexpr uint32_t XDES_ID = 0;
/** The list node data structure for the descriptors */
constexpr uint32_t XDES_FLST_NODE = 8;
/** contains state information of the extent */
constexpr uint32_t XDES_STATE = FLST_NODE_SIZE + 8;
/** Descriptor bitmap of the pages in the extent */
constexpr uint32_t XDES_BITMAP = FLST_NODE_SIZE + 12;

/*-------------------------------------*/

/** How many bits are there per page */
constexpr uint32_t XDES_BITS_PER_PAGE = 2;
/** Index of the bit which tells if the page is free */
constexpr uint32_t XDES_FREE_BIT = 0;
/** NOTE: currently not used! Index of the bit which tells if  there are old
versions of tuples on the page */
constexpr uint32_t XDES_CLEAN_BIT = 1;

/** States of a descriptor */
enum xdes_state_t {

  /** extent descriptor is not initialized */
  XDES_NOT_INITED = 0,

  /** extent is in free list of space */
  XDES_FREE = 1,

  /** extent is in free fragment list of space */
  XDES_FREE_FRAG = 2,

  /** extent is in full fragment list of space */
  XDES_FULL_FRAG = 3,

  /** extent belongs to a segment */
  XDES_FSEG = 4,

  /** fragment extent leased to segment */
  XDES_FSEG_FRAG = 5
};

/** File extent data structure size in bytes. */
#define XDES_SIZE \
  (XDES_BITMAP + UT_BITS_IN_BYTES(FSP_EXTENT_SIZE * XDES_BITS_PER_PAGE))

/** File extent data structure size in bytes for MAX page size. */
#define XDES_SIZE_MAX \
  (XDES_BITMAP + UT_BITS_IN_BYTES(FSP_EXTENT_SIZE_MAX * XDES_BITS_PER_PAGE))

/** File extent data structure size in bytes for MIN page size. */
#define XDES_SIZE_MIN \
  (XDES_BITMAP + UT_BITS_IN_BYTES(FSP_EXTENT_SIZE_MIN * XDES_BITS_PER_PAGE))

/** Offset of the descriptor array on a descriptor page */
constexpr uint32_t XDES_ARR_OFFSET = FSP_HEADER_OFFSET + FSP_HEADER_SIZE;

/** The number of reserved pages in a fragment extent. */
const ulint XDES_FRAG_N_USED = 2;

/** @} */

/** Initializes the file space system. */
void fsp_init(void);

/** Gets the size of the system tablespace from the tablespace header.  If
 we do not have an auto-extending data file, this should be equal to
 the size of the data files.  If there is an auto-extending data file,
 this can be smaller.
 @return size in pages */
page_no_t fsp_header_get_tablespace_size(void);

/** Calculate the number of pages to extend a datafile.
We extend single-table and general tablespaces first one extent at a time,
but 4 at a time for bigger tablespaces. It is not enough to extend always
by one extent, because we need to add at least one extent to FSP_FREE.
A single extent descriptor page will track many extents. And the extent
that uses its extent descriptor page is put onto the FSP_FREE_FRAG list.
Extents that do not use their extent descriptor page are added to FSP_FREE.
The physical page size is used to determine how many extents are tracked
on one extent descriptor page. See xdes_calc_descriptor_page().
@param[in]      page_size       page_size of the datafile
@param[in]      size            current number of pages in the datafile
@return number of pages to extend the file. */
page_no_t fsp_get_pages_to_extend_ibd(const page_size_t &page_size,
                                      page_no_t size);

/** Calculate the number of physical pages in an extent for this file.
@param[in]      page_size       page_size of the datafile
@return number of pages in an extent for this file. */
static inline page_no_t fsp_get_extent_size_in_pages(
    const page_size_t &page_size) {
  return (static_cast<page_no_t>(FSP_EXTENT_SIZE * UNIV_PAGE_SIZE /
                                 page_size.physical()));
}

/** Reads the space id from the first page of a tablespace.
 @return space id, ULINT UNDEFINED if error */
space_id_t fsp_header_get_space_id(
    const page_t *page); /*!< in: first page of a tablespace */

/** Read the server version number from the DD tablespace header.
@param[out]     version server version from tablespace header
@return false if success. */
bool fsp_header_dict_get_server_version(uint *version);

/** Read a tablespace header field.
@param[in]      page    first page of a tablespace
@param[in]      field   the header field
@return the contents of the header field */
inline uint32_t fsp_header_get_field(const page_t *page, uint32_t field) {
  return (mach_read_from_4(FSP_HEADER_OFFSET + field + page));
}

/** Update a tablespace header field.
@param[in]      page    first page of a tablespace
@param[in]      field   the header field
@param[in]      val     field value */
inline void fsp_header_set_field(page_t *page, uint32_t field, uint32_t val) {
  mach_write_to_4(page + FSP_HEADER_OFFSET + field, val);
}

/** Read the flags from the tablespace header page.
@param[in]      page    first page of a tablespace
@return the contents of FSP_SPACE_FLAGS */
inline uint32_t fsp_header_get_flags(const page_t *page) {
  return (fsp_header_get_field(page, FSP_SPACE_FLAGS));
}

/** Reads the page size from the first page of a tablespace.
@param[in]      page    first page of a tablespace
@return page size */
page_size_t fsp_header_get_page_size(const page_t *page);

/** Reads the encryption key from the first page of a tablespace.
@param[in]      fsp_flags       tablespace flags
@param[in,out]  e_key           tablespace key, iv
@param[in]      page            first page of a tablespace
@return true if success */
bool fsp_header_get_encryption_key(uint32_t fsp_flags, Encryption_key &e_key,
                                   page_t *page);

/** Get encryption operation type in progress from the first
page of a tablespace.
@param[in]      page            first page of a tablespace
@param[in]      page_size       tablespace page size
@return operation type
*/
Encryption::Progress fsp_header_encryption_op_type_in_progress(
    const page_t *page, page_size_t page_size);

/** Check if the tablespace size information is valid.
@param[in]      space_id        the tablespace identifier
@return true if valid, false if invalid. */
bool fsp_check_tablespace_size(space_id_t space_id);

/** Writes the space id and flags to a tablespace header.  The flags contain
 row type, physical/compressed page size, and logical/uncompressed page
 size of the tablespace. */
void fsp_header_init_fields(
    page_t *page,        /*!< in/out: first page in the space */
    space_id_t space_id, /*!< in: space id */
    uint32_t flags);     /*!< in: tablespace flags
                         (FSP_SPACE_FLAGS): 0, or
                         table->flags if newer than COMPACT */

/** Get the offset of encryption information in page 0.
@param[in]      page_size       page size.
@return offset on success, otherwise 0. */
ulint fsp_header_get_encryption_offset(const page_size_t &page_size);

/** Write the encryption info into the space header.
@param[in]      space_id                Tablespace id
@param[in]      space_flags             Tablespace flags
@param[in]      encrypt_info            Buffer for re-encrypt key
@param[in]      update_fsp_flags        If it need to update the space flags
@param[in]      rotate_encryption       If it is called during key rotation
@param[in,out]  mtr                     Mini-transaction
@return true if success. */
bool fsp_header_write_encryption(space_id_t space_id, uint32_t space_flags,
                                 byte *encrypt_info, bool update_fsp_flags,
                                 bool rotate_encryption, mtr_t *mtr);

/** Write the encryption progress info into the space header.
@param[in]      space_id                Tablespace id
@param[in]      space_flags             Tablespace flags
@param[in]      progress_info           Max pages (un)encrypted
@param[in]      operation_type          Type of operation
@param[in]      update_operation_type   Is operation to be updated
@param[in,out]  mtr                     Mini-transaction
@return true if success. */
bool fsp_header_write_encryption_progress(
    space_id_t space_id, ulint space_flags, ulint progress_info,
    byte operation_type, bool update_operation_type, mtr_t *mtr);

/** Rotate the encryption info in the space header.
@param[in]      space           Tablespace
@param[in]      encrypt_info    Buffer for re-encrypt key.
@param[in,out]  mtr             Mini-transaction
@return true if success. */
bool fsp_header_rotate_encryption(fil_space_t *space, byte *encrypt_info,
                                  mtr_t *mtr);

/** Initializes the space header of a new created space and creates also the
insert buffer tree root if space == 0.
@param[in]      space_id        Space id
@param[in]      size            Current size in blocks
@param[in,out]  mtr             Mini-transaction
@return true on success, otherwise false. */
bool fsp_header_init(space_id_t space_id, page_no_t size, mtr_t *mtr);

/** Increases the space size field of a space.
@param[in] space_id Space id
@param[in] size_inc Size increment in pages
@param[in,out] mtr Mini-transaction */
void fsp_header_inc_size(space_id_t space_id, page_no_t size_inc, mtr_t *mtr);

/** Creates a new segment.
 @return the block where the segment header is placed, x-latched, NULL
 if could not create segment because of lack of space */
buf_block_t *fseg_create(
    space_id_t space,  /*!< in: space id */
    page_no_t page,    /*!< in: page where the segment header is
                       placed: if this is != 0, the page must belong
                       to another segment, if this is 0, a new page
                       will be allocated and it will belong to the
                       created segment */
    ulint byte_offset, /*!< in: byte offset of the created
                       segment header on the page */
    mtr_t *mtr);       /*!< in/out: mini-transaction */

/** Creates a new segment.
@param[in] space_id Space id
@param[in,out] page Page where the segment header is placed: if this is != 0,
the page must belong to another segment, if this is 0, a new page will be
allocated and it will belong to the created segment
@param[in] byte_offset Byte offset of the created segment header on the page
@param[in,out] has_done_reservation True if the caller has already done the
reservation for the pages with fsp_reserve_free_extents (at least 2 extents: one
for the inode and the other for the segment) then there is no need to do the
check for this individual operation
@param[in,out] mtr Mini-transaction
@return the block where the segment header is placed, x-latched, NULL
if could not create segment because of lack of space */
buf_block_t *fseg_create_general(space_id_t space_id, page_no_t page,
                                 ulint byte_offset, bool has_done_reservation,
                                 mtr_t *mtr);

/** Calculates the number of pages reserved by a segment, and how many pages are
 currently used.
 @return number of reserved pages */
ulint fseg_n_reserved_pages(
    fseg_header_t *header, /*!< in: segment header */
    ulint *used,           /*!< out: number of pages used (<= reserved) */
    mtr_t *mtr);           /*!< in/out: mini-transaction */

/** Allocates a single free page from a segment. This function implements
 the intelligent allocation strategy which tries to minimize file space
 fragmentation.
@param[in,out] seg_header Segment header
@param[in] hint Hint of which page would be desirable
@param[in,out] direction If the new page is needed because of an index page
split, and records are inserted there in order, into which direction they go
alphabetically: fsp_down, fsp_up, fsp_no_dir
@param[in] has_done_reservation True if the caller has already done the
reservation for the page with fsp_reserve_free_extents, then there is no need to
do the check for this individual page
@param[in,out] mtr Mini-transaction
@param[in,out] init_mtr mtr or another mini-transaction in which
the page should be initialized. if init_mtr!=mtr, but the page is already
latched in mtr, do not initialize the page.
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr),
returned block is not allocated nor initialized otherwise */
[[nodiscard]] buf_block_t *fseg_alloc_free_page_general(
    fseg_header_t *seg_header, page_no_t hint, byte direction,
    bool has_done_reservation, mtr_t *mtr, mtr_t *init_mtr);

/** Allocates a single free page from a segment. This function implements
 the intelligent allocation strategy which tries to minimize
 file space fragmentation.
 @param[in,out] seg_header Segment header
 @param[in] hint Hint of which page would be desirable
 @param[in] direction If the new page is needed because
                                 of an index page split, and records are
                                 inserted there in order, into which
                                 direction they go alphabetically: FSP_DOWN,
                                 FSP_UP, FSP_NO_DIR
 @param[in,out] mtr Mini-transaction
 @return X-latched block, or NULL if no page could be allocated */
static inline buf_block_t *fseg_alloc_free_page(fseg_header_t *seg_header,
                                                page_no_t hint, byte direction,
                                                mtr_t *mtr) {
  return fseg_alloc_free_page_general(seg_header, hint, direction, false, mtr,
                                      mtr);
}

/** Reserves free pages from a tablespace. All mini-transactions which may
use several pages from the tablespace should call this function beforehand
and reserve enough free extents so that they certainly will be able
to do their operation, like a B-tree page split, fully. Reservations
must be released with function fil_space_release_free_extents!

The alloc_type below has the following meaning: FSP_NORMAL means an
operation which will probably result in more space usage, like an
insert in a B-tree; FSP_UNDO means allocation to undo logs: if we are
deleting rows, then this allocation will in the long run result in
less space usage (after a purge); FSP_CLEANING means allocation done
in a physical record delete (like in a purge) or other cleaning operation
which will result in less space usage in the long run. We prefer the latter
two types of allocation: when space is scarce, FSP_NORMAL allocations
will not succeed, but the latter two allocations will succeed, if possible.
The purpose is to avoid dead end where the database is full but the
user cannot free any space because these freeing operations temporarily
reserve some space.

Single-table tablespaces whose size is < FSP_EXTENT_SIZE pages are a special
case. In this function we would liberally reserve several extents for
every page split or merge in a B-tree. But we do not want to waste disk space
if the table only occupies < FSP_EXTENT_SIZE pages. That is why we apply
different rules in that special case, just ensuring that there are n_pages
free pages available.

@param[out]     n_reserved      Number of extents actually reserved; if we
                                return true and the tablespace size is <
                                FSP_EXTENT_SIZE pages, then this can be 0,
                                otherwise it is n_ext
@param[in]      space_id        Tablespace identifier
@param[in]      n_ext           Number of extents to reserve
@param[in]      alloc_type      Page reservation type (FSP_BLOB, etc)
@param[in,out]  mtr             Mini-transaction
@param[in]      n_pages         For small tablespaces (tablespace size is
                                less than FSP_EXTENT_SIZE), number of free
                                pages to reserve.
@return true if we were able to make the reservation */
bool fsp_reserve_free_extents(ulint *n_reserved, space_id_t space_id,
                              ulint n_ext, fsp_reserve_t alloc_type, mtr_t *mtr,
                              page_no_t n_pages = 2);

/** Calculate how many KiB of new data we will be able to insert to the
tablespace without running out of space.
@param[in]      space_id        tablespace ID
@return available space in KiB
@retval UINTMAX_MAX if unknown */
uintmax_t fsp_get_available_space_in_free_extents(space_id_t space_id);

/** Calculate how many KiB of new data we will be able to insert to the
tablespace without running out of space. Start with a space object that has
been acquired by the caller who holds it for the calculation,
@param[in]      space           tablespace object from fil_space_acquire()
@return available space in KiB */
uintmax_t fsp_get_available_space_in_free_extents(const fil_space_t *space);

/** Frees a single page of a segment.
@param[in] seg_header Segment header
@param[in] space_id Space id
@param[in] page Page offset
@param[in] ahi Whether we may need to drop the adaptive hash index
@param[in,out] mtr Mini-transaction */
void fseg_free_page(fseg_header_t *seg_header, space_id_t space_id,
                    page_no_t page, bool ahi, mtr_t *mtr);

/** Checks if a single page of a segment is free.
 @return true if free */
[[nodiscard]] bool fseg_page_is_free(
    fseg_header_t *seg_header, /*!< in: segment header */
    space_id_t space_id,       /*!< in: space id */
    page_no_t page);           /*!< in: page offset */
/** Frees part of a segment. This function can be used to free a segment
 by repeatedly calling this function in different mini-transactions.
 Doing the freeing in a single mini-transaction might result in
 too big a mini-transaction.
 @return true if freeing completed */
[[nodiscard]] bool fseg_free_step(
    fseg_header_t *header, /*!< in, own: segment header; NOTE: if the header
                           resides on the first page of the frag list
                           of the segment, this pointer becomes obsolete
                           after the last freeing step */
    bool ahi,              /*!< in: whether we may need to drop
                           the adaptive hash index */
    mtr_t *mtr);           /*!< in/out: mini-transaction */
/** Frees part of a segment. Differs from fseg_free_step because this function
 leaves the header page unfreed.
 @return true if freeing completed, except the header page */
[[nodiscard]] bool fseg_free_step_not_header(
    fseg_header_t *header, /*!< in: segment header which must reside on
                           the first fragment page of the segment */
    bool ahi,              /*!< in: whether we may need to drop
                           the adaptive hash index */
    mtr_t *mtr);           /*!< in/out: mini-transaction */

/** Checks if a page address is an extent descriptor page address.
@param[in]      page_id         page id
@param[in]      page_size       page size
@return true if a descriptor page */
static inline bool fsp_descr_page(const page_id_t &page_id,
                                  const page_size_t &page_size);

/** Parses a redo log record of a file page init.
@param[in] ptr Buffer.
@param[in] end_ptr Buffer end.
@param[in] block Block or nullptr.
@return end of log record or nullptr */
byte *fsp_parse_init_file_page(byte *ptr, byte *end_ptr, buf_block_t *block);

#ifdef UNIV_BTR_PRINT
/** Writes info of a segment. */
void fseg_print(fseg_header_t *header, /*!< in: segment header */
                mtr_t *mtr);           /*!< in/out: mini-transaction */
#endif                                 /* UNIV_BTR_PRINT */

/** Check whether a space id is an undo tablespace ID
Undo tablespaces have space_id's starting 1 less than the redo logs.
They are numbered down from this.  Since rseg_id=0 always refers to the
system tablespace, undo_space_num values start at 1.  The current limit
is 127. The translation from an undo_space_num is:
   undo space_id = log_first_space_id - undo_space_num
@param[in]      space_id        space id to check
@return true if it is undo tablespace else false. */
bool fsp_is_undo_tablespace(space_id_t space_id);

static inline bool fsp_is_system_tablespace(space_id_t space_id) {
  return (space_id == TRX_SYS_SPACE);
}

/** Check if the space_id is for a system-tablespace (shared + temp).
@param[in]      space_id        tablespace ID
@return true if id is a system tablespace, false if not. */
static inline bool fsp_is_system_or_temp_tablespace(space_id_t space_id) {
  return (fsp_is_system_tablespace(space_id) ||
          fsp_is_system_temporary(space_id));
}

/** Determine if the space ID is an IBD tablespace, either file_per_table
or a general shared tablespace, where user tables exist.
@param[in]      space_id        tablespace ID
@return true if it is a user tablespace ID */
static inline bool fsp_is_ibd_tablespace(space_id_t space_id) {
  return (space_id != TRX_SYS_SPACE && !fsp_is_undo_tablespace(space_id) &&
          !fsp_is_system_temporary(space_id));
}

/** Check if tablespace is file-per-table.
@param[in]      space_id        tablespace ID
@param[in]      fsp_flags       tablespace flags
@return true if tablespace is file-per-table. */
static inline bool fsp_is_file_per_table(space_id_t space_id,
                                         uint32_t fsp_flags) {
  return (!fsp_is_shared_tablespace(fsp_flags) &&
          fsp_is_ibd_tablespace(space_id));
}

/** Check if tablespace is dd tablespace.
@param[in]      space_id        tablespace ID
@return true if tablespace is dd tablespace. */
bool fsp_is_dd_tablespace(space_id_t space_id);

/** Determine if the tablespace is compressed from tablespace flags.
@param[in]      flags   Tablespace flags
@return true if compressed, false if not compressed */
static inline bool fsp_flags_is_compressed(uint32_t flags);

/** Determine if two tablespaces are equivalent or compatible.
@param[in]      flags1  First tablespace flags
@param[in]      flags2  Second tablespace flags
@return true the flags are compatible, false if not */
static inline bool fsp_flags_are_equal(uint32_t flags1, uint32_t flags2);

/** Initialize an FSP flags integer.
@param[in]      page_size       page sizes in bytes and compression flag.
@param[in]      atomic_blobs    Used by Dynamic and Compressed.
@param[in]      has_data_dir    This tablespace is in a remote location.
@param[in]      is_shared       This tablespace can be shared by many tables.
@param[in]      is_temporary    This tablespace is temporary.
@param[in]      is_encrypted    This tablespace is encrypted.
@return tablespace flags after initialization */
static inline uint32_t fsp_flags_init(const page_size_t &page_size,
                                      bool atomic_blobs, bool has_data_dir,
                                      bool is_shared, bool is_temporary,
                                      bool is_encrypted = false);

/** Convert a 32 bit integer tablespace flags to the 32 bit table flags.
This can only be done for a tablespace that was built as a file-per-table
tablespace. Note that the fsp_flags cannot show the difference between a
Compact and Redundant table, so an extra Compact boolean must be supplied.
                        Low order bit
                    | REDUNDANT | COMPACT | COMPRESSED | DYNAMIC
fil_space_t::flags  |     0     |    0    |     1      |    1
dict_table_t::flags |     0     |    1    |     1      |    1
@param[in]      fsp_flags       fil_space_t::flags
@param[in]      compact         true if not Redundant row format
@return tablespace flags (fil_space_t::flags) */
uint32_t fsp_flags_to_dict_tf(uint32_t fsp_flags, bool compact);

/** Calculates the descriptor index within a descriptor page.
@param[in]      page_size       page size
@param[in]      offset          page offset
@return descriptor index */
static inline ulint xdes_calc_descriptor_index(const page_size_t &page_size,
                                               ulint offset);

/** Gets a descriptor bit of a page.
@param[in]      descr   descriptor
@param[in]      bit     XDES_FREE_BIT or XDES_CLEAN_BIT
@param[in]      offset  page offset within extent: 0 ... FSP_EXTENT_SIZE - 1
@return true if free */
static inline bool xdes_get_bit(const xdes_t *descr, ulint bit,
                                page_no_t offset);

/** Calculates the page where the descriptor of a page resides.
@param[in]      page_size       page size
@param[in]      offset          page offset
@return descriptor page offset */
static inline page_no_t xdes_calc_descriptor_page(const page_size_t &page_size,
                                                  page_no_t offset);

/** Gets a pointer to the space header and acquires a
SX lock on the page.
@param[in]      id              Space id
@param[in]      page_size       Page size
@param[in,out]  mtr             Mini-transaction
@param[out]     block           Block
@return pointer to the space header, page x-locked */
fsp_header_t *fsp_get_space_header_block(space_id_t id,
                                         const page_size_t &page_size,
                                         mtr_t *mtr, buf_block_t **block);

/** Gets a pointer to the space header and acquires a
SX lock on the page.
@param[in]      id              Space id
@param[in]      page_size       Page size
@param[in,out]  mtr             Mini-transaction
@return pointer to the space header, page x-locked */
fsp_header_t *fsp_get_space_header(space_id_t id, const page_size_t &page_size,
                                   mtr_t *mtr);

/** Retrieve tablespace dictionary index root page number stored in the
page 0
@param[in]      space           Tablespace id
@param[in]      page_size       Page size
@param[in,out]  mtr             Mini-transaction
@return root page num of the tablespace dictionary index copy */
page_no_t fsp_sdi_get_root_page_num(space_id_t space,
                                    const page_size_t &page_size, mtr_t *mtr);

/** Write SDI Index root page num to page 0 of tablespace.
@param[in,out]  page            Page 0 frame
@param[in]      page_size       Size of page
@param[in]      root_page_num   Root page number of SDI
@param[in,out]  mtr             Mini-transaction */
void fsp_sdi_write_root_to_page(page_t *page, const page_size_t &page_size,
                                page_no_t root_page_num, mtr_t *mtr);

#include "fsp0fsp.ic"

/** Reads the server version from the first page of a tablespace.
@param[in]      page    first page of a tablespace
@return space server version */
inline uint32_t fsp_header_get_server_version(const page_t *page);

/** Reads the server space version from the first page of a tablespace.
@param[in]      page    first page of a tablespace
@return space server version */
inline uint32_t fsp_header_get_space_version(const page_t *page);

/** Get the state of an xdes.
@param[in]      descr   extent descriptor
@param[in,out]  mtr     Mini-transaction.
@return state */
inline xdes_state_t xdes_get_state(const xdes_t *descr, mtr_t *mtr) {
  ut_ad(descr && mtr);
  ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));

  const ulint state = mach_read_from_4(descr + XDES_STATE);

  ut_ad(xdes_state_is_valid(state));
  return (static_cast<xdes_state_t>(state));
}

#ifdef UNIV_DEBUG
/** Print the extent descriptor page in user-friendly format.
@param[in]  out     The output file stream
@param[in]  xdes    The extent descriptor page
@param[in]  page_no The page number of xdes page
@param[in]  mtr     Mini-transaction.
@return None. */
std::ostream &xdes_page_print(std::ostream &out, const page_t *xdes,
                              page_no_t page_no, mtr_t *mtr);

inline bool xdes_mem_t::is_valid() const {
  const ulint state = mach_read_from_4(m_xdes + XDES_STATE);
  return (xdes_state_is_valid(state));
}

inline const char *xdes_mem_t::state_name() const {
  const ulint val = mach_read_from_4(m_xdes + XDES_STATE);

  ut_ad(xdes_state_is_valid(val));

  xdes_state_t state = static_cast<xdes_state_t>(val);

  switch (state) {
    case XDES_NOT_INITED:
      return ("XDES_NOT_INITED");
    case XDES_FREE:
      return ("XDES_FREE");
    case XDES_FREE_FRAG:
      return ("XDES_FREE_FRAG");
    case XDES_FULL_FRAG:
      return ("XDES_FULL_FRAG");
    case XDES_FSEG:
      return ("XDES_FSEG");
    case XDES_FSEG_FRAG:
      return ("XDES_FSEG_FRAG");
  }
  return ("UNKNOWN");
}

#endif /* UNIV_DEBUG */

/** Update the tablespace size information and generate redo log for it.
@param[in]      header  Tablespace header.
@param[in]      size    New tablespace size in pages.
@param[in]      mtr     Mini-transaction context. */
inline void fsp_header_size_update(fsp_header_t *header, ulint size,
                                   mtr_t *mtr) {
  DBUG_TRACE;

  DBUG_LOG("ib_log", "old_size=" << mach_read_from_4(header + FSP_SIZE)
                                 << ", new_size=" << size);

  mlog_write_ulint(header + FSP_SIZE, size, MLOG_4BYTES, mtr);
}

/** Check if a specified page is inode page or not. This is used for
index root pages of hard-coded DD tables, we can safely assume that the passed
in page number is in the range of pages which are only either index root page
or inode page
@param[in]      page    Page number to check
@return true if it's inode page, otherwise false */
inline bool fsp_is_inode_page(page_no_t page);

/** Get the offset of SDI root page number in page 0.
@param[in]      page_size       Page size.
@return offset on success, else 0 */
inline ulint fsp_header_get_sdi_offset(const page_size_t &page_size);

/** Get the offset of encryption progress information in page 0.
@param[in]      page_size       page size.
@return offset on success, otherwise 0. */
inline ulint fsp_header_get_encryption_progress_offset(
    const page_size_t &page_size);

/** Determine if the tablespace has SDI.
@param[in]      space_id        Tablespace id
@return DB_SUCCESS if SDI is present else DB_ERROR
or DB_TABLESPACE_NOT_FOUND */
dberr_t fsp_has_sdi(space_id_t space_id);

/** Encrypt/Unencrypt a tablespace.
@param[in]      thd             current thread
@param[in]      space_id        Tablespace id
@param[in]      to_encrypt      true if to encrypt, false if to decrypt
@param[in,out]  dd_space_in     dd tablespace object
@return 0 for success, otherwise error code */
dberr_t fsp_alter_encrypt_tablespace(THD *thd, space_id_t space_id,
                                     bool to_encrypt, void *dd_space_in);

/** Initiate roll-forward of alter encrypt in background thread */
void fsp_init_resume_alter_encrypt_tablespace();

/** A wrapper class to operate on a file segment inode pointer (fseg_inode_t*)
 */
class File_segment_inode {
 public:
  /** Constructor
   @param[in]   space_id  Table space identifier
   @param[in]   page_size Size of each page in the tablespace.
   @param[in]   inode     File segment inode pointer
   @param[in]   mtr       Mini-transaction context. */
  File_segment_inode(space_id_t space_id, const page_size_t &page_size,
                     fseg_inode_t *inode, mtr_t *mtr)
      : m_space_id(space_id),
        m_page_size(page_size),
        m_fseg_inode(inode),
        m_mtr(mtr) IF_DEBUG(, m_random_engine(m_rd()), m_dist(1, 100)) {}

  /** Update the value of FSEG_NOT_FULL_N_USED.
  @param[in]   n_used  the new value of FSEG_NOT_FULL_N_USED. */
  void write_not_full_n_used(uint32_t n_used);

  /** Get the current value of FSEG_NOT_FULL_N_USED.
   @return the current value of FSEG_NOT_FULL_N_USED. */
  uint32_t read_not_full_n_used() const;

  /** Get the segment identifier value.
   @return the segment identifier value. */
  uint64_t get_seg_id() const {
    return (mach_read_from_8(m_fseg_inode + FSEG_ID));
  }

  /** Print the current object into the given output stream.
   @return the output stream. */
  std::ostream &print(std::ostream &out) const;

 private:
  /** Unique tablespace identifier */
  space_id_t m_space_id;

  /** The page size used in this tablespace. */
  const page_size_t &m_page_size;

  /** file segment inode pointer that is being wrapped by this object. */
  fseg_inode_t *m_fseg_inode;

  /** The mini-transaction operation context. */
  mtr_t *m_mtr;

#ifdef UNIV_DEBUG
 public:
  /** Verify the stored FSEG_NOT_FULL_N_USED value.
  @return true if correct value, false if incorrect. */
  bool verify_not_full_n_used();

 private:
  /** Calculate the value of FSEG_NOT_FULL_N_USED by traversing
  the FSEG_NOT_FULL list.
  @return the calculated value of FSEG_NOT_FULL_N_USED. */
  page_no_t calculate_not_full_n_used();

  std::random_device m_rd;
  std::default_random_engine m_random_engine;
  std::uniform_int_distribution<int> m_dist;

  /** To reduce the cost of verification of FSEG_NOT_FULL_N_USED, do it
  only when this function returns true.
  @return true for 10% of the time. */
  bool do_verify() { return (m_dist(m_random_engine) > 90); }
#endif /* UNIV_DEBUG */
};

/** The global output stream operator is overloaded to work with an object
 of type File_segment_inode.
@param[in]  out  the output stream.
@param[in]  obj  an object of type File_segment_inode.
@return  the output stream. */
inline std::ostream &operator<<(std::ostream &out,
                                const File_segment_inode &obj) {
  return (obj.print(out));
}

/** Returns the file segment inode, page x-latched.
@param[in]      header          Segment header
@param[in]      space           Space id
@param[in]      page_size       Page size
@param[in,out]  mtr             Mini-transaction
@param[out]     block           Inode block
@return segment inode, page x-latched */
fseg_inode_t *fseg_inode_get(const fseg_header_t *header, space_id_t space,
                             const page_size_t &page_size, mtr_t *mtr,
                             buf_block_t **block = nullptr);
#endif
