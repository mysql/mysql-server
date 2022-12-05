/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.

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
#include "fil0fil.h"
#include "log0recv.h"
#include "ut0byte.h"

/** Size of the doublewrite block in pages. */
#define DBLWR_V1_EXTENT_SIZE FSP_EXTENT_SIZE

/** Offset of the doublewrite buffer header on the trx system header page.  */
#define TRX_SYS_DBLWR_V1 (UNIV_PAGE_SIZE - 200)

/** 4-byte ver number which shows if we have created the doublewrite buffer. */
constexpr ulint DBLWR_VER = FSEG_HEADER_SIZE;

/** Page number of the first page in the first sequence of 64 (=
FSP_EXTENT_SIZE) consecutive pages in the doublewrite buffer. */
constexpr ulint DBLWR_V1_BLOCK1 = (4 + FSEG_HEADER_SIZE);

/** Page number of the first page in the second sequence of 64 consecutive
pages in the doublewrite buffer. */
constexpr ulint DBLWR_V1_BLOCK2 = (8 + FSEG_HEADER_SIZE);

namespace dblwr {

/** IO buffer in UNIV_PAGE_SIZE units and aligned on UNIV_PAGE_SIZE */
struct Buffer {
  /** Constructor
  @param[in]    n_pages                 Number of pages to create */
  explicit Buffer(size_t n_pages) noexcept
      : Buffer(n_pages, univ_page_size.physical()) {}

  /** Constructor
  @param[in]   n_pages   Number of pages to create
  @param[in]   phy_size  physical page size. */
  explicit Buffer(size_t n_pages, uint32_t phy_size) noexcept
      : m_phy_size(phy_size), m_n_bytes(n_pages * phy_size) {
    ut_a(n_pages > 0);
    m_ptr = static_cast<byte *>(ut::aligned_zalloc(m_n_bytes, phy_size));

    m_next = m_ptr;
  }

  /** Destructor */
  ~Buffer() noexcept {
    ut::aligned_free(m_ptr);
    m_ptr = nullptr;
  }

  /** Add the contents of ptr up to n_bytes to the buffer.
  @return false if it won't fit. Nothing is copied if it won't fit. */
  bool append(const void *ptr, size_t n_bytes) noexcept {
    ut_a(m_next >= m_ptr && m_next <= m_ptr + m_n_bytes);

    if (m_next + m_phy_size > m_ptr + m_n_bytes) {
      return false;
    }

    memcpy(m_next, ptr, n_bytes);
    m_next += m_phy_size;

    return true;
  }

  /** @return the start of the buffer to write from. */
  byte *begin() noexcept { return m_ptr; }

  /** @return the start of the buffer to write from. */
  const byte *begin() const noexcept { return m_ptr; }

  /** @return the size of of the buffer to write. */
  size_t size() const noexcept {
    ut_a(m_next >= m_ptr);
    return std::ptrdiff_t(m_next - m_ptr);
  }

  /** @return the capacity of the buffer in bytes. */
  size_t capacity() const noexcept { return m_n_bytes; }

  /** @return true if the buffer is empty. */
  bool empty() const noexcept { return size() == 0; }

  /** Empty the buffer. */
  void clear() noexcept { m_next = m_ptr; }

  /** Page size on disk (aka physical page size). */
  uint32_t m_phy_size;

  /** Write buffer used in writing to the doublewrite buffer,
  aligned to an address divisible by UNIV_PAGE_SIZE (which is
  required by Windows AIO) */
  byte *m_ptr{};

  /** Start of  next write to the buffer. */
  byte *m_next{};

  /** Size of the unaligned (raw) buffer. */
  const size_t m_n_bytes{};

  // Disable copying
  Buffer(const Buffer &) = delete;
  Buffer(const Buffer &&) = delete;
  Buffer &operator=(Buffer &&) = delete;
  Buffer &operator=(const Buffer &) = delete;
};

}  // namespace dblwr

#ifndef UNIV_HOTBACKUP

// Forward declaration
class buf_page_t;
class Double_write;

namespace dblwr {

/** Double write files location. */
extern std::string dir;

#ifdef UNIV_DEBUG
/** Crash the server after writing this page to the data file. */
extern page_id_t Force_crash;
#endif /* UNIV_DEBUG */

/** Startup the background thread(s) and create the instance.
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t open() noexcept;

/** Enable the doublewrite reduced mode by creating the necessary dblwr files
and in-memory structures
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t enable_reduced() noexcept;

/** Check and open the reduced doublewrite files if necessary
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t reduced_open() noexcept;

/** Shutdown the background thread and destroy the instance */
void close() noexcept;

/** Force a write of all pages in the queue.
@param[in] flush_type           FLUSH LIST or LRU_LIST flush request.
@param[in] buf_pool_index       Buffer pool instance for which called. */
void force_flush(buf_flush_t flush_type, uint32_t buf_pool_index) noexcept;

/** Force a write of all pages in all dblwr segments (reduced or regular)
This is used only when switching the doublewrite mode dynamically */
void force_flush_all() noexcept;

/** Writes a page to the doublewrite buffer on disk, syncs it,
then writes the page to the datafile.
@param[in]  flush_type          Flush type
@param[in]      bpage                       Buffer block to write
@param[in]      sync                        True if sync IO requested
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t write(buf_flush_t flush_type, buf_page_t *bpage,
                            bool sync) noexcept;

/** Obtain the encrypted frame and store it in bpage->m_io_frame
@param[in,out]  bpage  the buffer page containing the unencrypted frame.
@return the memory block containing the compressed + encrypted frame. */
[[nodiscard]] file::Block *get_encrypted_frame(buf_page_t *bpage) noexcept;

/** Updates the double write buffer when a write request is completed.
@param[in] bpage               Block that has just been writtent to disk.
@param[in] flush_type          Flush type that triggered the write. */
void write_complete(buf_page_t *bpage, buf_flush_t flush_type) noexcept;

/** Delete or adjust the dblwr file size if required. */
void reset_files() noexcept;

namespace v1 {
/** Read the boundaries of the legacy dblwr buffer extents.
@return DB_SUCCESS or error code. */
[[nodiscard]] dberr_t init() noexcept;

/** Create the dblwr data structures in the system tablespace.
@return DB_SUCCESS or error code. */
[[nodiscard]] dberr_t create() noexcept;

/** Check if the read is of a page inside the legacy dblwr buffer.
@param[in] page_no              Page number to check.
@return true if offset inside legacy dblwr buffer. */
[[nodiscard]] bool is_inside(page_no_t page_no) noexcept;

}  // namespace v1
}  // namespace dblwr

#endif /* !UNIV_HOTBACKUP */

namespace dblwr {

/** Number of pages per doublewrite thread/segment */
extern ulong n_pages;

const uint32_t REDUCED_BATCH_PAGE_SIZE = 8192;

/* 20-byte header.
Fields        : [batch id][checksum][data len][batch type][unused  ]
Field Width   : [4 bytes ][4 bytes ][2 bytes ][  1 byte  ][ 9 bytes]
Field Offsets : [   0    ][   4    ][    8   ][    10    ][   11   ] */
const uint32_t RB_BATCH_ID_SIZE = 4;
const uint32_t RB_CHECKSUM_SIZE = 4;
const uint32_t RB_DATA_LEN_SIZE = 2;
const uint32_t RB_BATCH_TYPE_SIZE = 1;
const uint32_t RB_UNUSED_BYTES_SIZE = 9;

/* Offsets of the header fields. */
constexpr const uint32_t RB_OFF_BATCH_ID = 0;
constexpr const uint32_t RB_OFF_CHECKSUM = RB_OFF_BATCH_ID + RB_BATCH_ID_SIZE;
constexpr const uint32_t RB_OFF_DATA_LEN = RB_OFF_CHECKSUM + RB_CHECKSUM_SIZE;
constexpr const uint32_t RB_OFF_BATCH_TYPE = RB_OFF_DATA_LEN + RB_DATA_LEN_SIZE;
constexpr const uint32_t RB_OFF_UNUSED = RB_OFF_BATCH_TYPE + RB_BATCH_TYPE_SIZE;

constexpr const uint32_t REDUCED_HEADER_SIZE =
    RB_BATCH_ID_SIZE     /* BATCH_ID */
    + RB_CHECKSUM_SIZE   /* CHECKSUM */
    + RB_DATA_LEN_SIZE   /* DATA_LEN */
    + RB_BATCH_TYPE_SIZE /* BATCH_TYPE */
    + 9 /* UNUSED BYTES */;

constexpr const uint32_t REDUCED_ENTRY_SIZE =
    sizeof(space_id_t) + sizeof(page_no_t) + sizeof(lsn_t);

constexpr const uint32_t REDUCED_DATA_SIZE =
    REDUCED_BATCH_PAGE_SIZE - REDUCED_HEADER_SIZE;

constexpr const uint32_t REDUCED_MAX_ENTRIES =
    REDUCED_DATA_SIZE / REDUCED_ENTRY_SIZE;

/** When --innodb-doublewrite=DETECT_ONLY, page contents are not written to the
dblwr buffer. Only the following Reduced_entry information is stored in the
dblwr buffer. */
struct Reduced_entry {
  space_id_t m_space_id;
  page_no_t m_page_no;
  lsn_t m_lsn;

  Reduced_entry(buf_page_t *bpage);

  Reduced_entry(space_id_t space_id, page_no_t page_no, lsn_t lsn)
      : m_space_id(space_id), m_page_no(page_no), m_lsn(lsn) {}

  byte *write(byte *ptr) {
    uint16_t offset = 0;
    mach_write_to_4(ptr + offset, m_space_id);
    offset += sizeof(m_space_id);

    mach_write_to_4(ptr + offset, m_page_no);
    offset += sizeof(m_page_no);

    mach_write_to_8(ptr + offset, m_lsn);
    offset += sizeof(m_lsn);

    return ptr + offset;
  }
};

struct Mode {
  /** The operating mode of doublewrite. The modes ON, TRUEE and
  DETECT_AND_RECOVER are equal to one another. The modes OFF and FALSEE are
  equal to one another.

  @note If you change order or add new values, please update
  innodb_doublewrite_names enum in handler/ha_innodb.cc */
  enum mode_t {
    /** Equal to FALSEE. In this mode, dblwr is disabled. */
    OFF,

    /** Equal to TRUEE and DETECT_AND_RECOVER modes. */
    ON,

    /** In this mode, dblwr is used only to detect torn writes.  At code level,
    this mode is also called as reduced mode. It is called reduced because the
    number of bytes written to the dblwr file is reduced in this mode. */
    DETECT_ONLY,

    /** This mode is synonymous with ON, TRUEE. */
    DETECT_AND_RECOVER,

    /** Equal to OFF mode. Intentionally wrong spelling because of compilation
    issues on Windows. */
    FALSEE,

    /** Equal to ON, DETECT_AND_RECOVER mode. Intentionally wrong spelling
    because of compilation issues on Windows platform. */
    TRUEE
  };

  /** Check if doublewrite is enabled.
  @param[in]     mode    dblwr ENUM
  @return true    if dblwr is enabled. */
  static inline bool is_enabled_low(ulong mode);

  /** Check if the doublewrite mode is disabled.
  @param[in]     mode    dblwr ENUM
  @return true if dblwr mode is OFF. */
  static inline bool is_disabled_low(ulong mode);

  /** Check if the doublewrite mode is detect-only (aka reduced).
  @param[in]     mode    dblwr ENUM
  @return true if dblwr mode is DETECT_ONLY. */
  static inline bool is_reduced_low(ulong mode);

  /** Check if the dblwr mode provides atomic writes.
  @return true if mode is ON, TRUEE or DETECT_AND_RECOVER.
  @return false if mode is OFF, FALSE or DETECT_ONLY. */
  static inline bool is_atomic(ulong mode);

  /** Convert the dblwr mode into a string representation.
  @param[in]  mode  the dblwr mode.
  @return string representation of the dblwr mode. */
  static const char *to_string(ulong mode);

  /** Check if the mode transition is from enabled to disabled.
  @param[in]  new_value  the new value of dblwr mode.
  @return true if mode transition is from enabled to disabled. */
  static inline bool is_enabled_to_disabled(ulong new_value);

  /** Check if the mode transition is from disabled to enabled.
  @param[in]  new_value  the new value of dblwr mode.
  @return true if mode transition is from disabled to enabled. */
  static inline bool is_disabled_to_enabled(ulong new_value);

  /** Check if the mode transition is equivalent.
  @param[in]  new_value  the new value of dblwr mode.
  @return true if mode transition is equivalent. */
  static bool is_same(ulong new_value);
};

bool Mode::is_atomic(ulong mode) {
  return mode == ON || mode == TRUEE || mode == DETECT_AND_RECOVER;
}

bool Mode::is_enabled_low(ulong mode) {
  return mode == ON || mode == TRUEE || mode == DETECT_AND_RECOVER ||
         mode == DETECT_ONLY;
}

bool Mode::is_disabled_low(ulong mode) { return mode == OFF || mode == FALSEE; }

bool Mode::is_reduced_low(ulong mode) { return mode == DETECT_ONLY; }

/** DBLWR mode. */
extern ulong g_mode;

/** true if DETECT_ONLY (aka reduced) mode is inited */
extern bool is_reduced_inited;

/** Check if doublewrite is enabled.
@return true    if dblwr mode is ON, DETECT_ONLY, DETECT_AND_RECOVER
@return false   if dblwr mode is OFF. */
inline bool is_enabled() { return Mode::is_enabled_low(g_mode); }

/** Check if the doublewrite mode is detect-only (aka reduced).
@return true if dblwr mode is DETECT_ONLY. */
inline bool is_reduced() { return Mode::is_reduced_low(g_mode); }

/** Check if the doublewrite mode is disabled.
@return true if dblwr mode is OFF. */
inline bool is_disabled() { return Mode::is_disabled_low(g_mode); }

bool Mode::is_enabled_to_disabled(ulong new_value) {
  return is_enabled() && Mode::is_disabled_low(new_value);
}

bool Mode::is_disabled_to_enabled(ulong new_value) {
  return is_disabled() && Mode::is_enabled_low(new_value);
}

/** @return string version of dblwr numeric values
@param[in]     mode    dblwr ENUM */
const char *to_string(ulong mode);

/** Number of files to use for the double write buffer. It must be <= than
the number of buffer pool instances. */
extern ulong n_files;

/** Maximum number of pages to write in one batch. */
extern ulong batch_size;

/** Toggle the doublewrite buffer. */
void set();

namespace recv {

class Pages;

/** Create the recovery dblwr data structures
@param[out]     pages           Pointer to newly created instance */
void create(Pages *&pages) noexcept;

/** Load the doublewrite buffer pages.
@param[in,out] pages           For storing the doublewrite pages read
                               from the double write buffer
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t load(Pages *pages) noexcept;

/** Load the doublewrite buffer pages.
@param[in,out] pages           For storing the doublewrite pages read
                               from the double write buffer
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t reduced_load(Pages *pages) noexcept;

/** Restore pages from the double write buffer to the tablespace.
@param[in,out]  pages  Pages from the doublewrite buffer
@param[in]   space  Tablespace pages to restore, if set to nullptr then try and
                    restore all.
@return DB_SUCCESS on success, error code on failure. */
dberr_t recover(Pages *pages, fil_space_t *space) noexcept;

/** Find a doublewrite copy of a page.
@param[in]      pages           Pages read from the doublewrite buffer
@param[in]      page_id         Page number to lookup
@return page frame
@retval NULL if no page was found */
[[nodiscard]] const byte *find(const Pages *pages,
                               const page_id_t &page_id) noexcept;

/** Find the LSN of the given page id  in the dblwr.
@param[in]     pages           Pages read from the doublewrite buffer
@param[in]     page_id         Page number to lookup
@return 0th element is true if page_id found in double write buffer.
@return 1st element is valid only if 0th element is true.
@return 1st element contains the LSN of the page in dblwr. */
[[nodiscard]] std::tuple<bool, lsn_t> find_entry(
    const Pages *pages, const page_id_t &page_id) noexcept;

/** Check if some pages from the double write buffer could not be
restored because of the missing tablespace IDs.
@param[in]      pages           Pages to check */
void check_missing_tablespaces(const Pages *pages) noexcept;

/** Free the recovery dblwr data structures
@param[out]     pages           Free the instance */
void destroy(Pages *&pages) noexcept;

/** Redo recovery configuration. */
class DBLWR {
 public:
  /** Constructor. */
  explicit DBLWR() noexcept { create(m_pages); }

  /** Destructor */
  ~DBLWR() noexcept { destroy(m_pages); }

  /** @return true if empty. */
  bool empty() const noexcept { return (m_pages == nullptr); }

  /** Load the doublewrite buffer pages. Doesn't create the doublewrite
  @return DB_SUCCESS or error code */
  [[nodiscard]] dberr_t load() noexcept { return (dblwr::recv::load(m_pages)); }

  /** Load the doublewrite buffer pages. Doesn't create the doublewrite
  @return DB_SUCCESS or error code */
  [[nodiscard]] dberr_t reduced_load() noexcept {
    return (dblwr::recv::reduced_load(m_pages));
  }

  /** Restore pages from the double write buffer to the tablespace.
  @param[in]    space           Tablespace pages to restore,
                                  if set to nullptr then try
                                  and restore all. */
  dberr_t recover(fil_space_t *space = nullptr) noexcept {
    return dblwr::recv::recover(m_pages, space);
  }

  /** Find a doublewrite copy of a page.
  @param[in]    page_id         Page number to lookup
  @return       page frame
  @retval nullptr if no page was found */
  [[nodiscard]] const byte *find(const page_id_t &page_id) noexcept {
    return (dblwr::recv::find(m_pages, page_id));
  }

  /** Find the LSN of the given page id  in the dblwr.
  @param[in]     page_id         Page number to lookup
  @return 0th element is true if page_id found in double write buffer.
  @return 1st element is valid only if 0th element is true.
  @return 1st element contains the LSN of the page in dblwr. */
  [[nodiscard]] std::tuple<bool, lsn_t> find_entry(
      const page_id_t &page_id) noexcept {
    return (dblwr::recv::find_entry(m_pages, page_id));
  }

  /** Check if some pages from the double write buffer
  could not be restored because of the missing tablespace IDs. */
  void check_missing_tablespaces() noexcept {
    dblwr::recv::check_missing_tablespaces(m_pages);
  }

#ifndef UNIV_HOTBACKUP
  /** Note that recovery is complete. Adjust the file sizes if necessary. */
  void recovered() noexcept { dblwr::reset_files(); }
#endif /* !UNIV_HOTBACKUP */

  /** Disably copying. */
  DBLWR(const DBLWR &) = delete;
  DBLWR(const DBLWR &&) = delete;
  DBLWR &operator=(DBLWR &&) = delete;
  DBLWR &operator=(const DBLWR &) = delete;

 private:
  /** Pages read from the double write file. */
  Pages *m_pages{};
};
}  // namespace recv

#ifdef UNIV_DEBUG
/** Check if the dblwr files contain encrypted pages.
@return true if dblwr file contains any encrypted pages,
        false if dblwr file contains no encrypted pages. */
[[nodiscard]] bool has_encrypted_pages() noexcept;
#endif /* UNIV_DEBUG */
}  // namespace dblwr

#endif /* buf0dblwr_h */
