/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.

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

/** @file include/buf0types.h
 The database buffer pool global types for the directory

 Created 11/17/1995 Heikki Tuuri
 *******************************************************/

#ifndef buf0types_h
#define buf0types_h

#include "os0event.h"
#include "sync0rw.h"
#include "ut0byte.h"
#include "ut0mutex.h"
#include "ut0rnd.h"
#include "ut0ut.h"

/** Magic value to use instead of checksums when they are disabled */
constexpr uint32_t BUF_NO_CHECKSUM_MAGIC = 0xDEADBEEFUL;

/** Buffer page (uncompressed or compressed) */
class buf_page_t;
/** Buffer block for which an uncompressed page exists */
struct buf_block_t;
/** Buffer pool chunk comprising buf_block_t */
struct buf_chunk_t;
/** Buffer pool comprising buf_chunk_t */
struct buf_pool_t;
/** Buffer pool statistics struct */
struct buf_pool_stat_t;
/** Buffer pool buddy statistics struct */
struct buf_buddy_stat_t;
/** Doublewrite memory struct */
struct buf_dblwr_t;
/** Flush observer for bulk create index */
class Flush_observer;

/** A buffer frame. @see page_t */
typedef byte buf_frame_t;

/** Flags for flush types */
enum buf_flush_t : uint8_t {
  /** Flush via the LRU list */
  BUF_FLUSH_LRU = 0,

  /** Flush via the flush list of dirty blocks */
  BUF_FLUSH_LIST,

  /** Flush via the LRU list but only a single page */
  BUF_FLUSH_SINGLE_PAGE,

  /** Index of last element + 1  */
  BUF_FLUSH_N_TYPES
};

/** Algorithm to remove the pages for a tablespace from the buffer pool.
See buf_LRU_flush_or_remove_pages(). */
enum buf_remove_t {
  /** Don't remove any pages. */
  BUF_REMOVE_NONE,

  /** Remove all pages from the buffer pool, don't write or sync to disk */
  BUF_REMOVE_ALL_NO_WRITE,

  /** Remove only from the flush list, don't write or sync to disk */
  BUF_REMOVE_FLUSH_NO_WRITE,

  /** Flush dirty pages to disk only don't remove from the buffer pool */
  BUF_REMOVE_FLUSH_WRITE
};

/** Flags for io_fix types */
enum buf_io_fix : uint8_t {
  /** no pending I/O */
  BUF_IO_NONE = 0,

  /** read pending */
  BUF_IO_READ,

  /** write pending */
  BUF_IO_WRITE,

  /** disallow relocation of block and its removal from the flush_list */
  BUF_IO_PIN
};

/** Alternatives for srv_checksum_algorithm, which can be changed by
setting innodb_checksum_algorithm */
enum srv_checksum_algorithm_t {
  SRV_CHECKSUM_ALGORITHM_CRC32,         /*!< Write crc32, allow crc32,
                                        innodb or none when reading */
  SRV_CHECKSUM_ALGORITHM_STRICT_CRC32,  /*!< Write crc32, allow crc32
                                        when reading */
  SRV_CHECKSUM_ALGORITHM_INNODB,        /*!< Write innodb, allow crc32,
                                        innodb or none when reading */
  SRV_CHECKSUM_ALGORITHM_STRICT_INNODB, /*!< Write innodb, allow
                                        innodb when reading */
  SRV_CHECKSUM_ALGORITHM_NONE,          /*!< Write none, allow crc32,
                                        innodb or none when reading */
  SRV_CHECKSUM_ALGORITHM_STRICT_NONE    /*!< Write none, allow none
                                        when reading */
};

/** Buffer pool resize status code and progress are tracked using these
atomic variables to ensure thread synchronization between
innodb_buffer_pool_size_update (raising srv_buf_resize_event) and
buf_resize_thread (handling srv_buf_resize_event) */
extern std::atomic_uint32_t buf_pool_resize_status_code;
extern std::atomic_uint32_t buf_pool_resize_status_progress;

/** Enumerate possible status codes during buffer pool resize. This is used
to identify the resize status using the corresponding code. */
enum buf_pool_resize_status_code_t {
  /** Resize completed or Resize not in progress*/
  BUF_POOL_RESIZE_COMPLETE = 0,

  /** Resize started */
  BUF_POOL_RESIZE_START = 1,

  /** Disabling Adaptive Hash Index */
  BUF_POOL_RESIZE_DISABLE_AHI = 2,

  /** Withdrawing blocks */
  BUF_POOL_RESIZE_WITHDRAW_BLOCKS = 3,

  /** Acquiring global lock */
  BUF_POOL_RESIZE_GLOBAL_LOCK = 4,

  /** Resizing pool */
  BUF_POOL_RESIZE_IN_PROGRESS = 5,

  /** Resizing hash */
  BUF_POOL_RESIZE_HASH = 6,

  /** Resizing failed */
  BUF_POOL_RESIZE_FAILED = 7
};

inline bool is_checksum_strict(srv_checksum_algorithm_t algo) {
  return (algo == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32 ||
          algo == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB ||
          algo == SRV_CHECKSUM_ALGORITHM_STRICT_NONE);
}

inline bool is_checksum_strict(ulint algo) {
  return (algo == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32 ||
          algo == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB ||
          algo == SRV_CHECKSUM_ALGORITHM_STRICT_NONE);
}

/** Parameters of binary buddy system for compressed pages (buf0buddy.h) */
/** @{ */
/** Zip shift value for the smallest page size */
constexpr uint32_t BUF_BUDDY_LOW_SHIFT = UNIV_ZIP_SIZE_SHIFT_MIN;

/** Smallest buddy page size */
constexpr uint32_t BUF_BUDDY_LOW = (1U << BUF_BUDDY_LOW_SHIFT);

/** Actual number of buddy sizes based on current page size */
#define BUF_BUDDY_SIZES (UNIV_PAGE_SIZE_SHIFT - BUF_BUDDY_LOW_SHIFT)

/** Maximum number of buddy sizes based on the max page size */
constexpr uint32_t BUF_BUDDY_SIZES_MAX =
    UNIV_PAGE_SIZE_SHIFT_MAX - BUF_BUDDY_LOW_SHIFT;

/** twice the maximum block size of the buddy system;
the underlying memory is aligned by this amount:
this must be equal to UNIV_PAGE_SIZE */
#define BUF_BUDDY_HIGH (BUF_BUDDY_LOW << BUF_BUDDY_SIZES)
/** @} */

typedef ib_bpmutex_t BPageMutex;
typedef ib_mutex_t BufListMutex;
typedef ib_mutex_t FlushListMutex;
typedef BPageMutex BufPoolZipMutex;
#ifndef UNIV_HOTBACKUP
typedef rw_lock_t BPageLock;
#endif /* !UNIV_HOTBACKUP */

/** Page identifier. */
class page_id_t {
 public:
  /**
  This class does not have a default constructor, because there is no natural
  choice for default values of m_space and m_page_no.

  If 0,0 were used, then it's not good as it doesn't match UINT32_UNDEFINED
  used to denote impossible page_no_t in several places, and 0 is a legal
  value for both space_id_t and page_id_t of a real page!

  If UINT32_UNDEFINED,UINT32_UNDEFINED were used, then it doesn't match the
  most common usage where use use memset(parent,0,sizeof(parent_t)); on a
  parent struct where one of the members has page_id_t type - which is ok
  given that page_id_t is TriviallyCopyable, and that the field is not
  used until it is assigned some real value. Such constructor would be
  misleading to people reading the code, as they might expect UINT32_UNDEFINED
  value, if they didn't notice the memset code buried somewhere in parent's
  initialization routine.

  Therefore, please either be explicit by using (space,page_no) overload,
  or continue to use memset at your own risk.
  */
  page_id_t() = delete;

  /** Constructor from (space, page_no).
  @param[in]    space   tablespace id
  @param[in]    page_no page number */
  page_id_t(space_id_t space, page_no_t page_no)
      : m_space(space), m_page_no(page_no) {}

  /** Retrieve the tablespace id.
  @return tablespace id */
  inline space_id_t space() const { return (m_space); }

  /** Retrieve the page number.
  @return page number */
  inline page_no_t page_no() const { return (m_page_no); }

  /** Retrieve the hash value.
  @return hashed value */
  inline uint64_t hash() const {
    constexpr uint64_t HASH_MASK = 1653893711;
    return (((uint64_t)m_space << 20) + m_space + m_page_no) ^ HASH_MASK;
  }

  /** Reset the values from a (space, page_no).
  @param[in]    space   tablespace id
  @param[in]    page_no page number */
  inline void reset(space_id_t space, page_no_t page_no) {
    m_space = space;
    m_page_no = page_no;
  }

  /** Reset the page number only.
  @param[in]    page_no page number */
  inline void set_page_no(page_no_t page_no) { m_page_no = page_no; }

  /** Check if a given page_id_t object is equal to the current one.
  @param[in]    a       page_id_t object to compare
  @return true if equal */
  inline bool operator==(const page_id_t &a) const {
    return (a.space() == m_space && a.page_no() == m_page_no);
  }

  /** Check if a given page_id_t object is not equal to the current one.
  @param[in]    a       page_id_t object to compare
  @return true if not equal */
  inline bool operator!=(const page_id_t &a) const { return !(*this == a); }

  /** Provides a lexicographic ordering on <space_id,page_no> pairs
  @param[in]    other   page_id_t object to compare
  @return true if this is strictly smaller than other */
  inline bool operator<(const page_id_t &other) const {
    return m_space < other.space() ||
           (m_space == other.space() && m_page_no < other.page_no());
  }

 private:
  /** Tablespace id. */
  space_id_t m_space;

  /** Page number. */
  page_no_t m_page_no;

  friend std::ostream &operator<<(std::ostream &out, const page_id_t &page_id);
};

/** Print the given page_id_t object.
@param[in,out]  out     the output stream
@param[in]      page_id the page_id_t object to be printed
@return the output stream */
std::ostream &operator<<(std::ostream &out, const page_id_t &page_id);

#endif /* buf0types.h */
