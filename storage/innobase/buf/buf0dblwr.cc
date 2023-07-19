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

/** @file buf/buf0dblwr.cc
Atomic writes handling. */

#include <sys/types.h>

#include "buf0buf.h"
#include "buf0checksum.h"
#include "log0chkp.h"
#include "os0enc.h"
#include "os0thread-create.h"
#include "page0zip.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "ut0mpmcbq.h"
#include "ut0mutex.h"
#include "ut0test.h"

#include <iomanip>
#include <iostream>
#include <regex>
#include <vector>

/** Doublewrite buffer */

/** fseg header of the fseg containing the doublewrite buffer */
constexpr ulint DBLWR_V1_FSEG = 0;

/** We repeat DBLWR_VER, DBLWR_V1_BLOCK1, DBLWR_V1_BLOCK2 so that if the trx
sys header is half-written to disk, we still may be able to recover the
information. */
constexpr ulint DBLWR_V1_REPEAT = 12;

/** If this is not yet set to DBLWR_V1_SPACE_ID_STORED_N,
we must reset the doublewrite buffer, because starting from 4.1.x the
space id of a data page is stored into FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID. */
constexpr ulint DBLWR_V1_SPACE_ID_STORED = (24 + FSEG_HEADER_SIZE);

/** Contents of DBLWR_VER. Legacy version, stores the blocks in the system
tablespace. */
constexpr ulint DBLWR_V1 = 536853855;

/** Contents of DBLWR_V1_SPACE_ID_STORED. */
constexpr ulint DBLWR_V1_SPACE_ID_STORED_N = 1783657386;

/** DBLWR file pages reserved per instance for single page flushes. */
constexpr uint32_t SYNC_PAGE_FLUSH_SLOTS = 512;

namespace dblwr {

std::string dir{"."};

ulong n_files{1};

ulong batch_size{};

ulong n_pages{64};

ulong g_mode{Mode::ON};

bool is_reduced_inited = false;

const char *Mode::to_string(ulong mode) {
  switch (mode) {
    case OFF:
      return ("OFF");
    case ON:
      return ("ON");
    case DETECT_ONLY:
      return ("DETECT_ONLY");
    case DETECT_AND_RECOVER:
      return ("DETECT_AND_RECOVER");
    case TRUEE:
      return "TRUE";
    case FALSEE:
      return "FALSE";
  }
  ut_ad(0);
  return ("");
}

bool Mode::is_same(ulong new_value) {
  switch (g_mode) {
    case OFF:
    case FALSEE:
      if (new_value == OFF || new_value == FALSEE) {
        return true;
      }
      break;
    case ON:
    case DETECT_AND_RECOVER:
    case TRUEE:
      if (new_value == ON || new_value == TRUEE ||
          new_value == DETECT_AND_RECOVER) {
        return true;
      }
      break;
    case DETECT_ONLY:
      if (new_value == DETECT_ONLY) {
        return true;
      }
      break;
  }
  return false;
}

/** Legacy dblwr buffer first segment page number. */
static page_no_t LEGACY_PAGE1;

/** Legacy dblwr buffer second segment page number. */
static page_no_t LEGACY_PAGE2;

inline bool is_odd(uint32_t val) { return val & 1; }

struct File {
  /** Check if the file ID is an odd number.
  @return true if the file ID is odd. */
  bool is_odd() const { return m_id & 1; }

  /** Files with odd number as ID are used for LRU batch segments.
  Also, only these odd files are used for LRU single segments.
  @return true if the file is to be used for LRU batch segments. */
  bool is_for_lru() const { return is_odd(); }

  /** ID of the file. */
  uint32_t m_id{};

  /** File name. */
  std::string m_name{};

  /** File handle. */
  pfs_os_file_t m_pfs{};

  /** Number of batched pages per doublwrite file. */
  static uint32_t s_n_pages;

  /** Serialize the object into JSON format.
  @return the object in JSON format. */
  [[nodiscard]] std::string to_json() const noexcept {
    std::ostringstream out;
    out << "{";
    out << "\"className\": \"dblwr::File\",";
    out << "\"m_id\": \"" << m_id << "\",";
    out << "\"m_name\": \"" << m_name << "\",";
    out << "\"s_n_pages\": \"" << s_n_pages << "\"";
    out << "}";

    return out.str();
  }

  /** Print this object into the given stream.
  @param[in]  out  output stream into which the current object is printed.
  @return the output stream. */
  std::ostream &print(std::ostream &out) const noexcept {
    out << to_json();
    return out;
  }
};

/** Overload the global output operator to work with dblwr::File type.
@param[in]  out  output stream into which the object is printed.
@param[in]  obj  An object to type dblwr::File.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const File &obj) noexcept {
  return obj.print(out);
}

uint32_t File::s_n_pages{};

#ifdef UNIV_DEBUG
/** Crash the server after writing this page to the data file. */
page_id_t Force_crash{UINT32_UNDEFINED, UINT32_UNDEFINED};
#endif /* UNIV_DEBUG */

namespace recv {

/** Page recovered from the doublewrite buffer */
struct Page {
  /** Constructor
  @param[in]    page_no           Page number in the doublewrite buffer
  @param[in]    page                Page read from the double write buffer
  @param[in]    n_bytes           Length of the page data. */
  Page(page_no_t page_no, const byte *page, uint32_t n_bytes)
      : m_no(page_no), m_buffer(1), m_recovered() {
    ut_a(n_bytes <= univ_page_size.physical());

    auto success = m_buffer.append(page, n_bytes);
    ut_a(success);
  }

  /** Page number in the doublewrite buffer. */
  page_no_t m_no{};

  /** Double write buffer page contents */
  dblwr::Buffer m_buffer;

  /** true if page was recovered. */
  bool m_recovered{};

  // Disable copying
  Page(const Page &) = delete;
  Page(const Page &&) = delete;
  Page &operator=(Page &&) = delete;
  Page &operator=(const Page &) = delete;
};

/** A record from reduced doublewrite buffer. */
struct Page_entry {
  Page_entry(space_id_t space_id, page_no_t page_no, lsn_t lsn)
      : m_space_id(space_id), m_page_no(page_no), m_lsn(lsn) {}

  /** Tablespace id */
  space_id_t m_space_id;

  /** Tablespace page number */
  page_no_t m_page_no;

  /** Page LSN */
  lsn_t m_lsn;
};

/** Pages recovered from the doublewrite buffer */
class Pages {
 public:
  using Buffers = std::vector<Page *, ut::allocator<Page *>>;
  using Page_entries = std::vector<Page_entry, ut::allocator<Page_entry>>;

  /** Default constructor */
  Pages() : m_pages(), m_page_entries() {}

  /** Destructor */
  ~Pages() noexcept {
    for (auto &page : m_pages) {
      ut::delete_(page);
    }

    m_pages.clear();
    m_page_entries.clear();
  }

  /** Add a page frame to the doublewrite recovery buffer.
  @param[in]    page_no                 Page number in the doublewrite buffer
  @param[in]    page                      Page contents
  @param[in]    n_bytes                 Size in bytes */
  void add(page_no_t page_no, const byte *page, uint32_t n_bytes) noexcept;

  /** Add a page entry from reduced doublewrite buffer to vector
  @param[in]   pg_entry        Reduced doublewrite buffer entry */
  void add_entry(Page_entry &pg_entry) { m_page_entries.push_back(pg_entry); }

  /** Find a doublewrite copy of a page.
  @param[in]    page_id                 Page number to lookup
  @return       page frame
  @retval nullptr if no page was found */
  const byte *find(const page_id_t &page_id) const noexcept;

  /** @return true if page is recovered from the regular doublewrite buffer
  @param[in]   page_id                 Page number to lookup */
  bool is_recovered(const page_id_t &page_id) const noexcept;

  /** Recover a page from the doublewrite buffer.
  @param[in]   dblwr_page_no         Page number if the doublewrite buffer
  @param[in]   space                 Tablespace the page belongs to
  @param[in]   page_no               Page number in the tablespace
  @param[in]   page                  Data to write to <space, page_no>
  @return true if page was restored to the tablespace */
  bool dblwr_recover_page(page_no_t dblwr_page_no, fil_space_t *space,
                          page_no_t page_no, byte *page) noexcept;

  /** Checks if page in tablespace is corrupted or an all-zero page
  @param[in]   space   Tablespace object
  @param[in]   page_id Page number to check for corruption
  @return tuple<0> - true if corrupted
          tuple<1> - true if the page is all zero page */
  std::tuple<bool, bool> is_actual_page_corrupted(fil_space_t *space,
                                                  page_id_t &page_id);

  /** Check if page was logged in reduced doublewrite buffer mode, if so also
  return the page LSN. Note if there are multiple entries of same page, we
  return the max_LSN of all entries.
  @param[in]   page_id                 Page number to lookup
  @retval tuple<0> - true if the page is found in reduced doublewrite
                     buffer, false if no page was found
          tuple<1> - if tuple<0> is true (ie page
                     found in reduced dblwr mode), then return the
                     max page LSN */
  std::tuple<bool, lsn_t> find_entry(const page_id_t &page_id) const noexcept {
    bool found = false;
    lsn_t max_lsn = 0;
    // There can be multiple entries with different LSNs, we are interested in
    // the entry with max_lsn
    for (auto &pe : m_page_entries) {
      if (page_id.space() == pe.m_space_id &&
          page_id.page_no() == pe.m_page_no) {
        if (pe.m_lsn > max_lsn) {
          found = true;
          max_lsn = pe.m_lsn;
        }
      }
    }

    return (std::tuple<bool, lsn_t>(found, max_lsn));
  }

  /** Recover double write buffer pages
  @param[in]  space  Tablespace pages to recover, if set to nullptr then try
                     and recovery all. */
  void recover(fil_space_t *space) noexcept;

  /** Check if some pages could be restored because of missing
  tablespace IDs */
  void check_missing_tablespaces() const noexcept;

  /** Object the vector of pages.
  @return the vector of pages. */
  [[nodiscard]] Buffers &get_pages() noexcept { return m_pages; }

 private:
  /** Check if page is logged in reduced doublewrite buffer. We cannot recover
  page because the entire page is not logged only an entry of
  space_id, page_id, LSN is logged. So we abort the server. It is expected
  that the user restores from backup
  @param[in]   space           Tablespace pages to check in reduced
  dblwr, if set to nullptr then try and recovery all. */
  void reduced_recover(fil_space_t *space) noexcept;

  /** Recovered doublewrite buffer page frames */
  Buffers m_pages;

  /** Page entries from reduced doublewrite buffer */
  Page_entries m_page_entries;

  // Disable copying
  Pages(const Pages &) = delete;
  Pages(const Pages &&) = delete;
  Pages &operator=(Pages &&) = delete;
  Pages &operator=(const Pages &) = delete;
};

#ifndef UNIV_HOTBACKUP
std::tuple<bool, bool> Pages::is_actual_page_corrupted(fil_space_t *space,
                                                       page_id_t &page_id) {
  auto result = std::make_tuple(false, false);

  if (page_id.page_no() >= space->size) {
    /* Do not report the warning if the tablespace is going to be truncated.
     */
    if (!undo::is_active(space->id)) {
      ib::warn(ER_IB_MSG_DBLWR_1313)
          << "Page# " << page_id.page_no()
          << " stored in the doublewrite file is"
             " not within data file space bounds "
          << space->size << " bytes:  page : " << page_id;
    }
    return (result);
  }

  Buffer buffer{1};
  const page_size_t page_size(space->flags);

  /* We want to ensure that for partial reads the
  unread portion of the page is NUL. */
  memset(buffer.begin(), 0x0, page_size.physical());

  IORequest request;

  request.dblwr();

  /* Read in the page from the data file to compare. */
  auto err = fil_io(request, true, page_id, page_size, 0, page_size.physical(),
                    buffer.begin(), nullptr);

  if (err != DB_SUCCESS) {
    ib::warn(ER_IB_MSG_DBLWR_1314)
        << "Double write fle recovery: " << page_id << " read failed with "
        << "error: " << ut_strerr(err);
  }

  /* Is the page read from the data file corrupt? */
  BlockReporter data_file_page(true, buffer.begin(), page_size,
                               fsp_is_checksum_disabled(space->id));

  bool is_all_zero = buf_page_is_zeroes(buffer.begin(), page_size);

  std::get<0>(result) = data_file_page.is_corrupted();
  std::get<1>(result) = is_all_zero;
  return (result);
}
#endif /* !UNIV_HOTBACKUP */

}  // namespace recv
}  // namespace dblwr

using namespace dblwr;
using namespace dblwr::recv;

#ifndef UNIV_HOTBACKUP

// Forward declaration.
class Segment;
class Batch_segment;

/** Doublewrite implementation. Assumes it can use DBLWR_PAGES. */
class Double_write {
 public:
  /** Number of instances. */
  static uint32_t s_n_instances;

  /** For collecting pages to write. */
  struct Buf_pages {
    /** Constructor.
    @param[in] size             Number of pages to reserve. */
    explicit Buf_pages(uint32_t size) : m_pages(size) {
      ut_a(size > 0);
      ut_a(m_pages.capacity() == size);
      ut_a(m_pages.size() == m_pages.capacity());
    }

    /** Add a page to the collection.
    @param[in] bpage     Page to write.
    @param[in] e_block   encrypted block. */
    void push_back(buf_page_t *bpage, const file::Block *e_block) noexcept {
      ut_a(m_size < m_pages.capacity());
#ifdef UNIV_DEBUG
      {
        byte *e_frame =
            (e_block == nullptr) ? nullptr : os_block_get_frame(e_block);
        if (e_frame != nullptr) {
          ut_ad(mach_read_from_4(e_frame + FIL_PAGE_OFFSET) ==
                bpage->page_no());
          ut_ad(mach_read_from_4(e_frame + FIL_PAGE_SPACE_ID) ==
                bpage->space());
        }
      }
#endif /* UNIV_DEBUG */
      m_pages[m_size++] = std::make_tuple(bpage, e_block);
    }

    /** Clear the collection. */
    void clear() noexcept { m_size = 0; }

    /** @return check if collection is empty. */
    bool empty() const noexcept { return size() == 0; }

    /** @return number of active elements. */
    uint32_t size() const noexcept { return m_size; }

    /** @return the capacity of the collection. */
    [[nodiscard]] uint32_t capacity() const noexcept {
      return m_pages.capacity();
    }

    typedef std::tuple<buf_page_t *, const file::Block *> Dblwr_tuple;
    using Pages = std::vector<Dblwr_tuple, ut::allocator<Dblwr_tuple>>;

    /** Collection of pages. */
    Pages m_pages{};

    /** Number of live elements. */
    uint32_t m_size{};
  };

  /** Constructor
  @param[in] id                 Instance ID
  @param[in] n_pages            Number of pages handled by this instance. */
  Double_write(uint16_t id, uint32_t n_pages) noexcept;

  /** Destructor */
  virtual ~Double_write() noexcept;

  /** @return instance ID */
  [[nodiscard]] uint16_t id() const noexcept { return m_id; }

  /** Process the requests in the flush queue, write the blocks to the
  double write file, sync the file if required and then write to the
  data files. */
  void write(buf_flush_t flush_type) noexcept;

  /** @return the double write instance to use for flushing.
  @param[in] buf_pool_index     Buffer pool instance number.
  @param[in] flush_type         LRU or Flush list write.
  @param[in] is_reduced         true if reduced mode of dblwr is being used.
  @return instance that will handle the flush to disk. */
  [[nodiscard]] static Double_write *instance(buf_flush_t flush_type,
                                              uint32_t buf_pool_index,
                                              bool is_reduced) noexcept {
    ut_a(buf_pool_index < srv_buf_pool_instances);

    auto midpoint = s_instances->size() / 2;
    auto i = midpoint > 0 ? buf_pool_index % midpoint : 0;

    if (flush_type == BUF_FLUSH_LIST) {
      i += midpoint;
    }

    return (is_reduced ? s_r_instances->at(i) : s_instances->at(i));
  }

  /** Wait for any pending batch to complete.
  @return true if the thread had to wait for another batch. */
  bool wait_for_pending_batch() noexcept {
    ut_ad(mutex_own(&m_mutex));

    auto sig_count = os_event_reset(m_event);

    std::atomic_thread_fence(std::memory_order_acquire);

    if (m_batch_running.load(std::memory_order_acquire)) {
      mutex_exit(&m_mutex);

      MONITOR_INC(MONITOR_DBLWR_FLUSH_WAIT_EVENTS);
      os_event_wait_low(m_event, sig_count);
      sig_count = os_event_reset(m_event);
      return true;
    }

    return false;
  }

  /** Flush buffered pages to disk, clear the buffers.
  @param[in] flush_type           FLUSH LIST or LRU LIST flush.
  @return false if there was a write batch already in progress. */
  bool flush_to_disk(buf_flush_t flush_type) noexcept {
    ut_ad(mutex_own(&m_mutex));

    /* Wait for any batch writes that are in progress. */
    if (wait_for_pending_batch()) {
      ut_ad(!mutex_own(&m_mutex));
      return false;
    }

    MONITOR_INC(MONITOR_DBLWR_FLUSH_REQUESTS);

    /* Write the pages to disk and free up the buffer. */
    write_pages(flush_type);

    ut_a(m_buffer.empty());
    ut_a(m_buf_pages.empty());

    return true;
  }

  /** Process the requests in the flush queue, write the blocks to the
  double write file, sync the file if required and then write to the
  data files.
  @param[in] flush_type         LRU or FLUSH request. */
  void write_pages(buf_flush_t flush_type) noexcept;

  virtual uint16_t write_dblwr_pages(buf_flush_t flush_type) noexcept;

  void write_data_pages(buf_flush_t flush_type, uint16_t batch_id) noexcept;

  /** Force a flush of the page queue.
  @param[in] flush_type           FLUSH LIST or LRU LIST flush. */
  void force_flush(buf_flush_t flush_type) noexcept {
    for (;;) {
      mutex_enter(&m_mutex);
      if (!m_buf_pages.empty() && !flush_to_disk(flush_type)) {
        ut_ad(!mutex_own(&m_mutex));
        continue;
      }
      break;
    }
    mutex_exit(&m_mutex);
  }

  /** Add a page to the flush batch. If the flush batch is full then write
  the batch to disk.
  @param[in] flush_type     Flush type.
  @param[in] bpage          Page to flush to disk.
  @param[in] e_block        Encrypted block frame or nullptr. */
  void enqueue(buf_flush_t flush_type, buf_page_t *bpage,
               const file::Block *e_block) noexcept {
    ut_ad(buf_page_in_file(bpage));

    void *frame{};
    uint32_t len{};
    byte *e_frame =
        (e_block == nullptr) ? nullptr : os_block_get_frame(e_block);

    if (e_frame != nullptr) {
      frame = e_frame;
      len = e_block->m_size;
    } else {
      prepare(bpage, &frame, &len);
    }

    ut_a(len <= univ_page_size.physical());

    for (;;) {
      mutex_enter(&m_mutex);

      if (m_buffer.append(frame, len)) {
        break;
      }

      if (flush_to_disk(flush_type)) {
        auto success = m_buffer.append(frame, len);
        ut_a(success);
        break;
      }

      ut_ad(!mutex_own(&m_mutex));
    }

    m_buf_pages.push_back(bpage, e_block);

    mutex_exit(&m_mutex);
  }

  /** Note that the IO batch has started. */
  void batch_started() noexcept {
    m_batch_running.store(true, std::memory_order_release);
  }

  /** Wake up all the threads that were waiting for the batch to complete. */
  void batch_completed() noexcept {
    m_batch_running.store(false, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_release);
    os_event_set(m_event);
  }

  /** Create the batch write segments.
  @param[in] segments_per_file  Number of configured segments per file.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] static dberr_t create_batch_segments(
      uint32_t segments_per_file) noexcept;

  /** Create the Reduced batch write segments. These segments are mapped
  to separate file which has extension .bdblwr
  @return DB_SUCCESS or error code. */
  [[nodiscard]] static dberr_t create_reduced_batch_segments() noexcept;

  /** Create the single page flush segments.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] static dberr_t create_single_segments() noexcept;

  /** Get the instance that handles a particular page's IO. Submit the
  write request to the a double write queue that is empty.
  @param[in]  flush_type        Flush type.
  @param[in]    bpage             Page from the buffer pool.
  @param[in]  e_block    compressed + encrypted frame contents or nullptr.*/
  static void submit(buf_flush_t flush_type, buf_page_t *bpage,
                     const file::Block *e_block) noexcept {
    if (s_instances == nullptr) {
      return;
    }

    auto dblwr = instance(flush_type, bpage);
    dblwr->enqueue(flush_type, bpage, e_block);
  }

  /** Writes a single page to the doublewrite buffer on disk, syncs it,
  then writes the page to the datafile.
  @param[in]    bpage             Data page to write to disk.
  @param[in]    e_block           Encrypted data block.
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t sync_page_flush(buf_page_t *bpage,
                                               file::Block *e_block) noexcept;

  /** @return the double write instance to use for flushing.
  @param[in] flush_type         LRU or Flush list write.
  @param[in] bpage              Page to write to disk.
  @return instance that will handle the flush to disk. */
  [[nodiscard]] static Double_write *instance(
      buf_flush_t flush_type, const buf_page_t *bpage) noexcept {
    return instance(flush_type, buf_pool_index(buf_pool_from_bpage(bpage)),
                    is_reduced());
  }

  /** Updates the double write buffer when a write request is completed.
  @param[in,out] bpage          Block that has just been written to disk.
  @param[in] flush_type         Flush type that triggered the write. */
  static void write_complete(buf_page_t *bpage,
                             buf_flush_t flush_type) noexcept;

  /** REad the V1 doublewrite buffer extents boundaries.
  @param[in,out] block1         Starting block number for the first extent.
  @param[in,out] block2         Starting block number for the second extent.
  @return true if successful, false if not. */
  [[nodiscard]] static bool init_v1(page_no_t &block1,
                                    page_no_t &block2) noexcept;

  /** Creates the V1 doublewrite buffer extents. The header of the
  doublewrite buffer is placed on the trx system header page.
  @param[in,out] block1         Starting block number for the first extent.
  @param[in,out] block2         Starting block number for the second extent.
  @return true if successful, false if not. */
  [[nodiscard]] static bool create_v1(page_no_t &block1,
                                      page_no_t &block2) noexcept;

  /** Writes a page that has already been written to the
  doublewrite buffer to the data file. It is the job of the
  caller to sync the datafile.
  @param[in]  in_bpage          Page to write.
  @param[in]  sync              true if it's a synchronous write.
  @param[in]  e_block           block containing encrypted data frame.
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t write_to_datafile(
      const buf_page_t *in_bpage, bool sync,
      const file::Block *e_block) noexcept;

  /** Force a flush of the page queue.
  @param[in] flush_type           FLUSH LIST or LRU LIST flush.
  @param[in] buf_pool_index       Buffer pool instance for which called. */
  static void force_flush(buf_flush_t flush_type,
                          uint32_t buf_pool_index) noexcept {
    if (s_instances == nullptr) {
      return;
    }
    auto dblwr = instance(flush_type, buf_pool_index, false);

    dblwr->force_flush(flush_type);

    if (is_reduced_inited) {
      auto dblwr = instance(flush_type, buf_pool_index, true);
      dblwr->force_flush(flush_type);
    }
  }

  /** Load the doublewrite buffer pages from an external file.
  @param[in,out]        file                  File handle
  @param[in,out]        pages                 For storing the doublewrite pages
                                read from the file
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t load(dblwr::File &file,
                                    recv::Pages *pages) noexcept;

  /** Load the reduced doublewrite buffer page entries from an reduced batch
  double write buffer file (.bdblwr)
  @param[in,out]       file    File handle
  @param[in,out]       pages   For storing the doublewrite pages
                                read from the file
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t load_reduced_batch(dblwr::File &file,
                                                  recv::Pages *pages) noexcept;

  /** Write zeros to the file if it is "empty"
  @param[in]  file   File instance.
  @param[in]  n_pages   Size in physical pages.
  @param[in]  phy_size  Physical page size in DBLWR file (For reduced DBLW
              file, it is not UNIV_PAGE_SIZE, it is hardcoded to 8K page size)
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t init_file(
      dblwr::File &file, uint32_t n_pages,
      uint32_t phy_size = univ_page_size.physical()) noexcept;

  /** Reset the size in bytes to the configured size.
  @param[in,out] file                                           File to reset.
  @param[in] truncate           Truncate the file to configured size if true. */
  static void reset_file(dblwr::File &file, bool truncate) noexcept;

  /** Reset the size in bytes to the configured size.
  @param[in,out] file                  File to reset
  @param[in]    pages_per_file         Number of pages to be created
                                        in doublewrite file
  @param[in]    phy_size               physical page size */
  static void reduced_reset_file(dblwr::File &file, uint32_t pages_per_file,
                                 uint32_t phy_size) noexcept;

  /** Reset the size in bytes to the configured size of all files. */
  static void reset_files() noexcept {
    for (auto &file : Double_write::s_files) {
      /* Physically truncate the file: true. */
      Double_write::reset_file(file, true);
    }
  }

  /** Create the v2 data structures
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t create_v2() noexcept;

  /** Create the data structures for reduced doublewrite buffer
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t create_reduced() noexcept;

#ifndef _WIN32
  /** @return true if we need to fsync to disk */
  [[nodiscard]] static bool is_fsync_required() noexcept {
    /* srv_unix_file_flush_method is a dynamic variable. */
    return srv_unix_file_flush_method != SRV_UNIX_O_DIRECT &&
           srv_unix_file_flush_method != SRV_UNIX_O_DIRECT_NO_FSYNC;
  }
#endif /* _WIN32 */

  /** Extract the data and length to write to the doublewrite file
  @param[in]    bpage                     Page to write
  @param[out]   ptr                         Start of buffer to write
  @param[out]   len                         Length of the data to write */
  static void prepare(const buf_page_t *bpage, void **ptr,
                      uint32_t *len) noexcept;

  /** Free the data structures. */
  static void shutdown() noexcept;

  /** Toggle the doublewrite buffer dynamically
  @param[in]  value  Current value */
  static void toggle(ulong value) noexcept {
    if (s_instances == nullptr) {
      return;
    }

    if (Mode::is_atomic(value)) {
      ib::info(ER_IB_MSG_DBLWR_1304) << "Atomic write enabled";
    } else {
      ib::info(ER_IB_MSG_DBLWR_1305) << "Atomic write disabled";
    }
  }

  /** Write the data to disk synchronously.
  @param[in]    segment      Segment to write to.
  @param[in]    bpage        Page to write.
  @param[in]    e_block      Encrypted block.  Can be nullptr. */
  static void single_write(Segment *segment, const buf_page_t *bpage,
                           file::Block *e_block) noexcept;

 private:
  /** Create the singleton instance, start the flush thread
  @return DB_SUCCESS or error code */
  [[nodiscard]] static dberr_t start() noexcept;

  /** Asserts when a corrupt block is found during writing out
  data to the disk.
  @param[in]    block                     Block that was corrupt */
  static void croak(const buf_block_t *block) noexcept;

  /** Check the LSN values on the page with which this block
  is associated.  Also validate the page if the option is set.
  @param[in]    block                     Block to check */
  static void check_block(const buf_block_t *block) noexcept;

  /** Check the LSN values on the page.
  @param[in]    page                      Page to check */
  static void check_page_lsn(const page_t *page) noexcept;

  /** Calls buf_page_get() on the TRX_SYS_PAGE and returns
  a pointer to the doublewrite buffer within it.
  @param[in,out]        mtr                     To manage the page latches
  @return pointer to the doublewrite buffer within the filespace
          header page. */
  [[nodiscard]] static byte *get(mtr_t *mtr) noexcept;

 protected:
  using Segments = mpmc_bq<Segment *>;
  using Instances = std::vector<Double_write *>;
  using Batch_segments = mpmc_bq<Batch_segment *>;

  /** Instance ID */
  uint16_t m_id{};

  /** Protects m_buf_pages. */
  ib_mutex_t m_mutex;

  /** Wait for IO batch to complete. */
  os_event_t m_event;

  /** true if the the batch hasn't completed yet. */
  std::atomic_bool m_batch_running{false};

  /** The copy of the page frame, the page must be in in m_buf_pages. */
  Buffer m_buffer;

  /** Pages that should be written to the data files. */
  Buf_pages m_buf_pages;

  /** File segments to use for LRU batched writes. */
  static Batch_segments *s_LRU_batch_segments;

  /** File segments to use for flush list batched writes. */
  static Batch_segments *s_flush_list_batch_segments;

  /** File segments to use for single page writes. */
  static Segments *s_single_segments;

  /** File segments to use for LRU batched writes in reduced dblwr mode */
  static Batch_segments *s_r_LRU_batch_segments;

  /** File segments to use for flush list batched writes (reduced mode) */
  static Batch_segments *s_r_flush_list_batch_segments;

  /** For indexing batch segments by ID. */
  static std::vector<Batch_segment *> s_segments;

  /** Utility function to free batch segments
  @param[in]  segments  batch segment to free */
  static void free_segments(Batch_segments *&segments) noexcept;

  /** Last used batch_id for regular batch segments. Any id greater
  than this belongs to reduced double write */
  static uint32_t s_regular_last_batch_id;

  /** @return true if batch belonged to reduced dblwr. When returning
  a batch segment to lock-free queue, we should know which lock-free
  queue(Batch_segments) to return to
  @param[in]  batch_id Batch segment id */
  static bool is_reduced_batch_id(uint32_t batch_id);

 public:
  /** Files to use for atomic writes. */
  static std::vector<dblwr::File> s_files;

  /** Reduced batch doublewrite files to use for atomic writes. */
  static std::vector<dblwr::File> s_r_files;

  /** The global instances */
  static Instances *s_instances;

  /** The global Reduced Doublewrite instances */
  static Instances *s_r_instances;

  // Disable copying
  Double_write(const Double_write &) = delete;
  Double_write(const Double_write &&) = delete;
  Double_write &operator=(Double_write &&) = delete;
  Double_write &operator=(const Double_write &) = delete;

  /** Number of bytes written to disk by this instance.  It is never reset.
  It is printed to server log during shutdown. */
  unsigned long long m_bytes_written{};
};

/** File segment of a double write file. */
class Segment {
 public:
  /** Constructor.
  @param[in] file               File that owns the segment.
  @param[in] start              Offset (page number) of segment in the file.
  @param[in] n_pages            Number of pages in the segment. */
  Segment(dblwr::File &file, page_no_t start, uint32_t n_pages)
      : m_file(file),
        m_phy_size(univ_page_size.physical()),
        m_start(start * m_phy_size),
        m_end(m_start + (n_pages * m_phy_size)) {}

  /** Constructor.
  @param[in] file               File that owns the segment.
  @param[in] phy_size          Size of an entry in segment
  @param[in] start              Offset (page number) of segment in the file.
  @param[in] n_pages            Number of pages in the segment. */
  Segment(dblwr::File &file, uint32_t phy_size, page_no_t start,
          uint32_t n_pages)
      : m_file(file),
        m_phy_size(phy_size),
        m_start(start * m_phy_size),
        m_end(m_start + (n_pages * m_phy_size)) {}

  /** Destructor. */
  virtual ~Segment() = default;

  /** Write to the segment.
  @param[in] ptr                Start writing from here.
  @param[in] len                Number of bytes to write. */
  void write(const void *ptr, uint32_t len) noexcept {
    ut_a(len <= m_end - m_start);
    IORequest req(IORequest::WRITE | IORequest::DO_NOT_WAKE);

    req.dblwr();

    auto err = os_file_write_retry(req, m_file.m_name.c_str(), m_file.m_pfs,
                                   ptr, m_start, len);
    ut_a(err == DB_SUCCESS);
  }

  /** Flush the segment to disk. */
  void flush() noexcept { os_file_flush(m_file.m_pfs); }

  /** File that owns the segment. */
  dblwr::File &m_file;

  /** Physical page size of each entry/Segment */
  uint32_t m_phy_size{};

  /** Physical offset in the file for the segment. */
  os_offset_t m_start{};

  /** Physical offset up to which this segment is responsible for. */
  os_offset_t m_end{};

  // Disable copying
  Segment(Segment &&) = delete;
  Segment(const Segment &) = delete;
  Segment &operator=(Segment &&) = delete;
  Segment &operator=(const Segment &) = delete;
};

/** Segment for batched writes. */
class Batch_segment : public Segment {
 public:
  /** Constructor.
  @param[in] id                 Segment ID.
  @param[in] file               File that owns the segment.
  @param[in] start              Offset (page number) of segment in the file.
  @param[in] n_pages            Number of pages in the segment. */
  Batch_segment(uint16_t id, dblwr::File &file, page_no_t start,
                uint32_t n_pages)
      : Segment(file, start, n_pages), m_id(id) {
    reset();
  }

  /** Constructor.
  @param[in] id                 Segment ID.
  @param[in] file               File that owns the segment.
  @param[in] phy_size          physical size of each segment entry
  @param[in] start              Offset (page number) of segment in the file.
  @param[in] n_pages            Number of pages in the segment. */
  Batch_segment(uint16_t id, dblwr::File &file, uint32_t phy_size,
                page_no_t start, uint32_t n_pages)
      : Segment(file, phy_size, start, n_pages), m_id(id) {
    reset();
  }

  /** Destructor. */
  ~Batch_segment() noexcept override {
    ut_a(m_uncompleted.load(std::memory_order_relaxed) == 0);
    ut_a(m_batch_size == 0);
  }

  /** @return the batch segment ID. */
  uint16_t id() const noexcept { return m_id; }

  /**  Write a batch to the segment.
  @param[in] buffer             Buffer to write. */
  void write(const Buffer &buffer) noexcept;

  /**  Write a batch to the segment.
  @param[in] buf    Buffer to write
  @param[in] len    amount of data to write */
  void write(const byte *buf, uint32_t len) noexcept;

  /** Called on page write completion.
  @return if batch ended. */
  [[nodiscard]] bool write_complete() noexcept {
    /* We "release our reference" here, so can't access the segment after this
    fetch_sub() unless we decreased it to 0 and handle requeuing it. */
    const auto n = m_uncompleted.fetch_sub(1, std::memory_order_relaxed);
    ut_ad(0 < n);
    return n == 1;
  }

  /** Reset the state. */
  void reset() noexcept {
    /* We shouldn't reset() the batch while it's being processed. */
    ut_ad(m_uncompleted.load(std::memory_order_relaxed) == 0);
    m_uncompleted.store(0, std::memory_order_relaxed);
    m_batch_size = 0;
  }

  /** Set the batch size.
  @param[in] size               Number of pages to write to disk. */
  void set_batch_size(uint32_t size) noexcept {
    /* We should only call set_batch_size() on new or reset()ed instance. */
    ut_ad(m_uncompleted.load(std::memory_order_relaxed) == 0);
    ut_ad(m_batch_size == 0);
    m_batch_size = size;
    m_uncompleted.store(size, std::memory_order_relaxed);
  }

  /** @return the batch size. */
  uint32_t batch_size() const noexcept { return m_batch_size; }

  /** Note that the batch has started for the double write instance.
  @param[in] dblwr              Instance for which batch has started. */
  void start(Double_write *dblwr) noexcept {
    m_dblwr = dblwr;
    m_dblwr->batch_started();
  }

  /** Note that the batch has completed. */
  void completed() noexcept {
    m_dblwr->batch_completed();
    m_dblwr = nullptr;
  }

  /** Batch segment ID. */
  uint16_t m_id{};

  /** The instance that is being written to disk. */
  Double_write *m_dblwr{};

  byte m_pad1[ut::INNODB_CACHE_LINE_SIZE];

  /** Size of the batch.
  Set to number of pages to be written with set_batch_size() before scheduling
  writes to data pages.
  Reset to zero with reset() after all IOs are completed.
  Read only by the thread which has observed the last IO completion, the one
  which will reset it back to zero and enqueue the segment for future reuse.
  Accesses to this field are ordered by happens-before relation:
  set_batch_size() sequenced-before
    fil_io()  happens-before
    dblwr::write_complete() entry sequenced-before
  batch_size() sequenced-before
  reset() sequenced-before
    enqueue() synchronizes-with
    dequeue() sequenced-before
  set_batch_size() ...
  */
  uint32_t m_batch_size{};

  byte m_pad2[ut::INNODB_CACHE_LINE_SIZE];

  /** Number of page writes in the batch which are still not completed.
  Set to equal m_batch_size by set_batch_size(), and decremented when a page
  write is finished (either by failing/not attempting or in IO completion).
  It serves a role of a reference counter: when it drops to zero, the segment
  can be enqueued back to the pool of available segments.
  Accessing a segment which has m_uncompleted == 0 is safe only from the thread
  which knows it can not be recycled - for example because it's the thread which
  has caused the m_uncompleted drop to 0 and will enqueue it, or it's the thread
  which has just dequeued it, or it is handling shutdown.*/
  std::atomic_int m_uncompleted{};
};

/** Reduced doublewrite implementation. Uses separate
.bdblwr files and can coexist with regular doublewrite buffer
implementation */
class Reduced_double_write : public Double_write {
 public:
  /** Constructor
  @param[in] id                 Instance ID
  @param[in] n_pages            Number of pages handled by this instance. */
  Reduced_double_write(uint16_t id, uint32_t n_pages)
      : Double_write(id, n_pages), m_buf(nullptr), m_page(nullptr) {}

  /** Destructor */
  ~Reduced_double_write() override {
    if (m_buf != nullptr) {
      ut::free(m_buf);
    }
  }
  /** Process the requests in the flush queue, write the space_id, page_id, LSN
  to the reduced double write file (.bdblwr), sync the file if required and
  then write to the data files.
  @param[in] flush_type         LRU or FLUSH request. */
  virtual uint16_t write_dblwr_pages(buf_flush_t flush_type) noexcept override;

  /** Allocate a temporary buffer for writing page entries.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t allocate();

 private:
  /** Clear the temporary buffer used for writing reduced dblwr page */
  void clear() { memset(m_page, 0, REDUCED_BATCH_PAGE_SIZE); }

  /** Create Reduced dblwr page header
  @param[in]   batch_id        Batch_id of the Reduced dblwr segment
  @param[in]   checksum        Checksum of the page
  @param[in]   data_len        Length of data in page
  @param[in]   flush_type      LRU of FLUSH_LIST type*/
  void create_header(uint32_t batch_id, uint32_t checksum, uint16_t data_len,
                     buf_flush_t flush_type) {
    mach_write_to_4(m_page + RB_OFF_BATCH_ID, batch_id);
    mach_write_to_4(m_page + RB_OFF_CHECKSUM, checksum);
    mach_write_to_2(m_page + RB_OFF_DATA_LEN, data_len);
    mach_write_to_1(m_page + RB_OFF_BATCH_TYPE, flush_type);
    memset(m_page + RB_OFF_UNUSED, 0, RB_UNUSED_BYTES_SIZE);
  }

  /** Calculate checksum for the Reduced dblwr page
  @param[in]   data_len        amount of data in page
  @return checksum calcuated */
  uint32_t calculate_checksum(uint16_t data_len) {
    return (ut_crc32(m_page + REDUCED_HEADER_SIZE, data_len));
  }

 private:
  /** Un-aligned temporary buffer */
  byte *m_buf;

  /** aligned temporary buffer. Created from m_buf */
  byte *m_page;
};

dberr_t Reduced_double_write::allocate() {
  ut_ad(m_buf == nullptr);

  m_buf = static_cast<byte *>(ut::zalloc(2 * REDUCED_BATCH_PAGE_SIZE));

  if (m_buf == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  /* Align the memory for file i/o if we might have O_DIRECT set */
  m_page = static_cast<byte *>(ut_align(m_buf, REDUCED_BATCH_PAGE_SIZE));

  return DB_SUCCESS;
}

uint16_t Reduced_double_write::write_dblwr_pages(
    buf_flush_t flush_type) noexcept {
  ut_ad(mutex_own(&m_mutex));
  ut_a(!m_buffer.empty());
  ut_ad(m_buf != nullptr);

  uint16_t data_len{};
  byte *ptr = m_page + REDUCED_HEADER_SIZE;
#ifdef UNIV_DEBUG
  byte *ptr_orig = ptr;
  const byte *end_ptr = m_page + REDUCED_BATCH_PAGE_SIZE;
  const size_t bytes_needed = m_buf_pages.size() * REDUCED_ENTRY_SIZE;
  ut_ad(bytes_needed <= REDUCED_DATA_SIZE);
#endif /* UNIV_DEBUG */
  for (uint32_t i = 0; i < m_buf_pages.size(); ++i) {
    const auto bpage = std::get<0>(m_buf_pages.m_pages[i]);

    Reduced_entry entry(bpage);
    ut_ad(ptr + REDUCED_ENTRY_SIZE < end_ptr);
    ptr = entry.write(ptr);
    data_len += REDUCED_ENTRY_SIZE;
  }
  ut_ad(ptr - ptr_orig == data_len);
  ut_ad(data_len <= REDUCED_DATA_SIZE);

  // Calculate checksum for the data written
  uint32_t checksum = calculate_checksum(data_len);

  Batch_segment *batch_segment{};

  auto segments = flush_type == BUF_FLUSH_LRU ? s_r_LRU_batch_segments
                                              : s_r_flush_list_batch_segments;

  while (!segments->dequeue(batch_segment)) {
    std::this_thread::yield();
  }

  // Create Page header
  create_header(batch_segment->id(), checksum, data_len, flush_type);

  ut_ad(data_len / REDUCED_ENTRY_SIZE == m_buf_pages.size());

  batch_segment->start(this);

  batch_segment->write(m_page, REDUCED_BATCH_PAGE_SIZE);

  m_bytes_written += REDUCED_BATCH_PAGE_SIZE;

  m_buffer.clear();
  clear();

#ifndef _WIN32
  if (is_fsync_required()) {
    batch_segment->flush();
  }
#endif /* !_WIN32 */

  batch_segment->set_batch_size(m_buf_pages.size());
  return batch_segment->id();
}

uint32_t Double_write::s_n_instances{};
std::vector<dblwr::File> Double_write::s_files;
std::vector<dblwr::File> Double_write::s_r_files;
Double_write::Segments *Double_write::s_single_segments{};
Double_write::Batch_segments *Double_write::s_LRU_batch_segments{};
Double_write::Batch_segments *Double_write::s_flush_list_batch_segments{};
std::vector<Batch_segment *> Double_write::s_segments{};

Double_write::Instances *Double_write::s_instances{};
uint32_t Double_write::s_regular_last_batch_id{};

Double_write::Batch_segments *Double_write::s_r_LRU_batch_segments{};
Double_write::Batch_segments *Double_write::s_r_flush_list_batch_segments{};
Double_write::Instances *Double_write::s_r_instances{};

Double_write::Double_write(uint16_t id, uint32_t n_pages) noexcept
    : m_id(id), m_buffer(n_pages), m_buf_pages(n_pages) {
  ut_a(n_pages == dblwr::n_pages);
  ut_a(m_buffer.capacity() / UNIV_PAGE_SIZE == m_buf_pages.capacity());

  mutex_create(LATCH_ID_DBLWR, &m_mutex);
  m_event = os_event_create();
}

Double_write::~Double_write() noexcept {
  mutex_free(&m_mutex);
  os_event_destroy(m_event);
}

void Double_write::prepare(const buf_page_t *bpage, void **ptr,
                           uint32_t *len) noexcept {
  auto block = reinterpret_cast<const buf_block_t *>(bpage);
  auto state = buf_block_get_state(block);

  /* No simple validate for compressed pages exists. */
  if (state == BUF_BLOCK_FILE_PAGE && block->page.zip.data == nullptr) {
    /* Check that the actual page in the buffer pool is
    not corrupt and the LSN values are sane. */
    check_block(block);
  }

  if (bpage->size.is_compressed()) {
    UNIV_MEM_ASSERT_RW(bpage->zip.data, bpage->size.physical());

    *ptr = bpage->zip.data;
    *len = bpage->size.physical();

  } else {
    if (state != BUF_BLOCK_FILE_PAGE) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_DBLWR_1297)
          << "Invalid page state: state: " << static_cast<unsigned>(state)
          << " block state: "
          << static_cast<unsigned>(buf_page_get_state(bpage));
    } else {
      ut_ad(state == buf_block_get_state(block));
    }

    *ptr =
        reinterpret_cast<buf_block_t *>(const_cast<buf_page_t *>(bpage))->frame;

    UNIV_MEM_ASSERT_RW(*ptr, bpage->size.logical());

    *len = bpage->size.logical();
  }
}

void Double_write::single_write(Segment *segment, const buf_page_t *bpage,
                                file::Block *e_block) noexcept {
  uint32_t len{};
  void *frame{};

  if (e_block != nullptr) {
    frame = os_block_get_frame(e_block);
    len = e_block->m_size;
  } else {
    prepare(bpage, &frame, &len);
  }

  ut_ad(len <= univ_page_size.physical());

  segment->write(frame, len);
}

void Batch_segment::write(const Buffer &buffer) noexcept {
  Segment::write(buffer.begin(), buffer.size());
}

void Batch_segment::write(const byte *buf, uint32_t len) noexcept {
  Segment::write(buf, len);
}

dberr_t Double_write::create_v2() noexcept {
  ut_a(!s_files.empty());
  ut_a(s_instances == nullptr);

  s_instances = ut::new_withkey<Instances>(UT_NEW_THIS_FILE_PSI_KEY);

  if (s_instances == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  dberr_t err{DB_SUCCESS};

  for (uint32_t i = 0; i < s_n_instances; ++i) {
    auto ptr = ut::new_withkey<Double_write>(UT_NEW_THIS_FILE_PSI_KEY, i,
                                             dblwr::n_pages);

    if (ptr == nullptr) {
      err = DB_OUT_OF_MEMORY;
      break;
    }

    s_instances->push_back(ptr);
  }

  if (err != DB_SUCCESS) {
    for (auto &dblwr : *s_instances) {
      ut::delete_(dblwr);
    }
    ut::delete_(s_instances);
    s_instances = nullptr;
  }

  return err;
}

dberr_t Double_write::create_reduced() noexcept {
  ut_a(!s_files.empty());
  ut_a(s_r_instances == nullptr);

  s_r_instances = ut::new_<Instances>();

  if (s_r_instances == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  dberr_t err{DB_SUCCESS};

  for (uint32_t i = 0; i < s_n_instances; ++i) {
    auto ptr = ut::new_<Reduced_double_write>(i, dblwr::n_pages);

    if (ptr == nullptr) {
      err = DB_OUT_OF_MEMORY;
      break;
    }

    err = ptr->allocate();

    if (err != DB_SUCCESS) {
      return err;
    }

    s_r_instances->push_back(ptr);
  }

  if (err != DB_SUCCESS) {
    for (auto &dblwr : *s_r_instances) {
      ut::delete_(dblwr);
    }
    ut::delete_(s_r_instances);
    s_r_instances = nullptr;
  }

  return err;
}

void Double_write::free_segments(Batch_segments *&segments) noexcept {
  if (segments != nullptr) {
    Batch_segment *s{};
    while (segments->dequeue(s)) {
      ut::delete_(s);
    }
    ut::delete_(segments);
    segments = nullptr;
  }
}

void Double_write::shutdown() noexcept {
  if (s_instances == nullptr) {
    return;
  }

  unsigned long long n_bytes = 0;
  for (auto dblwr : *s_instances) {
    n_bytes += dblwr->m_bytes_written;
    ut::delete_(dblwr);
  }

  ib::info(ER_IB_DBLWR_BYTES_INFO)
      << "Bytes written to disk by DBLWR (ON): " << n_bytes;

  n_bytes = 0;
  if (s_r_instances != nullptr) {
    for (auto dblwr : *s_r_instances) {
      n_bytes += dblwr->m_bytes_written;
      ut::delete_(dblwr);
    }

    ib::info(ER_IB_RDBLWR_BYTES_INFO)
        << "Bytes written to disk by DBLWR (REDUCED): " << n_bytes;
  }

  for (auto &file : s_files) {
    if (file.m_pfs.m_file != OS_FILE_CLOSED) {
      os_file_close(file.m_pfs);
    }
  }

  s_files.clear();

  for (auto &file : s_r_files) {
    if (file.m_pfs.m_file != OS_FILE_CLOSED) {
      os_file_close(file.m_pfs);
    }
  }

  s_r_files.clear();

  free_segments(s_LRU_batch_segments);
  free_segments(s_flush_list_batch_segments);
  free_segments(s_r_LRU_batch_segments);
  free_segments(s_r_flush_list_batch_segments);

  if (s_single_segments != nullptr) {
    Segment *s{};
    while (s_single_segments->dequeue(s)) {
      ut::delete_(s);
    }
    ut::delete_(s_single_segments);
    s_single_segments = nullptr;
  }

  ut::delete_(s_instances);
  ut::delete_(s_r_instances);
  s_instances = nullptr;
  s_r_instances = nullptr;
}

void Double_write::check_page_lsn(const page_t *page) noexcept {
  if (memcmp(
          page + (FIL_PAGE_LSN + 4),
          page + (univ_page_size.physical() - FIL_PAGE_END_LSN_OLD_CHKSUM + 4),
          4)) {
    const uint32_t lsn1 = mach_read_from_4(page + FIL_PAGE_LSN + 4);

    const uint32_t lsn2 = mach_read_from_4(page + univ_page_size.physical() -
                                           FIL_PAGE_END_LSN_OLD_CHKSUM + 4);

    ib::error(ER_IB_MSG_111) << "The page to be written seems corrupt!"
                                " The low 4 bytes of LSN fields do not match"
                                " ("
                             << lsn1 << " != " << lsn2
                             << ")!"
                                " Noticed in the buffer pool.";
  }
}

void Double_write::croak(const buf_block_t *block) noexcept {
  buf_page_print(block->frame, univ_page_size, BUF_PAGE_PRINT_NO_CRASH);

  ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_112)
      << "Apparent corruption of an index page " << block->page.id
      << " to be written to data file. We intentionally crash"
         " the server to prevent corrupt data from ending up in"
         " data files.";
}

void Double_write::check_block(const buf_block_t *block) noexcept {
  ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);

  check_page_lsn(block->frame);

  switch (fil_page_get_type(block->frame)) {
    case FIL_PAGE_INDEX:
    case FIL_PAGE_RTREE:
    case FIL_PAGE_SDI:
      if (page_is_comp(block->frame)) {
        if (page_simple_validate_new(block->frame)) {
          return;
        }
      } else if (page_simple_validate_old(block->frame)) {
        return;
      }
      /* While it is possible that this is not an index page
      but just happens to have wrongly set FIL_PAGE_TYPE,
      such pages should never be modified to without also
      adjusting the page type during page allocation or
      buf_flush_init_for_writing() or fil_page_reset_type(). */
      break;
    case FIL_PAGE_TYPE_FSP_HDR:
    case FIL_PAGE_IBUF_BITMAP:
    case FIL_PAGE_TYPE_UNKNOWN:
    /* Do not complain again, we already reset this field. */
    case FIL_PAGE_UNDO_LOG:
    case FIL_PAGE_INODE:
    case FIL_PAGE_IBUF_FREE_LIST:
    case FIL_PAGE_TYPE_SYS:
    case FIL_PAGE_TYPE_TRX_SYS:
    case FIL_PAGE_TYPE_XDES:
    case FIL_PAGE_TYPE_BLOB:
    case FIL_PAGE_TYPE_ZBLOB:
    case FIL_PAGE_TYPE_ZBLOB2:
    case FIL_PAGE_SDI_BLOB:
    case FIL_PAGE_SDI_ZBLOB:
    case FIL_PAGE_TYPE_LOB_INDEX:
    case FIL_PAGE_TYPE_LOB_DATA:
    case FIL_PAGE_TYPE_LOB_FIRST:
    case FIL_PAGE_TYPE_ZLOB_FIRST:
    case FIL_PAGE_TYPE_ZLOB_DATA:
    case FIL_PAGE_TYPE_ZLOB_INDEX:
    case FIL_PAGE_TYPE_ZLOB_FRAG:
    case FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY:
    case FIL_PAGE_TYPE_RSEG_ARRAY:
    case FIL_PAGE_TYPE_LEGACY_DBLWR:

      /* TODO: validate also non-index pages */
      return;

    case FIL_PAGE_TYPE_ALLOCATED:
      /* Empty pages should never be flushed. Unless we are creating the
      legacy doublewrite buffer.  */
      break;
  }

  croak(block);
}

dberr_t Double_write::write_to_datafile(const buf_page_t *in_bpage, bool sync,
                                        const file::Block *e_block) noexcept {
  ut_ad(buf_page_in_file(in_bpage));
  ut_ad(in_bpage->current_thread_has_io_responsibility());
  ut_ad(in_bpage->is_io_fix_write());
  uint32_t len;
  void *frame{};

  if (e_block == nullptr) {
    Double_write::prepare(in_bpage, &frame, &len);
  } else {
    frame = os_block_get_frame(e_block);
    len = e_block->m_size;
  }

  /* Our IO API is common for both reads and writes and is
  therefore geared towards a non-const parameter. */
  auto bpage = const_cast<buf_page_t *>(in_bpage);

  uint32_t type = IORequest::WRITE;

  if (sync) {
    type |= IORequest::DO_NOT_WAKE;
  }

  IORequest io_request(type);
  io_request.set_encrypted_block(e_block);

#ifdef UNIV_DEBUG
  {
    byte *page = static_cast<byte *>(frame);
    ut_ad(mach_read_from_4(page + FIL_PAGE_OFFSET) == bpage->page_no());
    ut_ad(mach_read_from_4(page + FIL_PAGE_SPACE_ID) == bpage->space());
  }
#endif /* UNIV_DEBUG */

  io_request.set_original_size(bpage->size.physical());
  auto err =
      fil_io(io_request, sync, bpage->id, bpage->size, 0, len, frame, bpage);

  /* When a tablespace is deleted with BUF_REMOVE_NONE, fil_io() might
  return DB_PAGE_IS_STALE or DB_TABLESPACE_DELETED. */
  ut_a(err == DB_SUCCESS || err == DB_TABLESPACE_DELETED ||
       err == DB_PAGE_IS_STALE);

  return err;
}

dberr_t Double_write::sync_page_flush(buf_page_t *bpage,
                                      file::Block *e_block) noexcept {
#ifdef UNIV_DEBUG
  ut_d(auto page_id = bpage->id);

  if (dblwr::Force_crash == page_id) {
    auto frame = reinterpret_cast<const buf_block_t *>(bpage)->frame;
    const auto p = reinterpret_cast<byte *>(frame);

    ut_ad(page_get_space_id(p) == dblwr::Force_crash.space());
    ut_ad(page_get_page_no(p) == dblwr::Force_crash.page_no());
  }
#endif /* UNIV_DEBUG */

  Segment *segment{};

  while (!s_single_segments->dequeue(segment)) {
    std::this_thread::yield();
  }

  single_write(segment, bpage, e_block);

#ifndef _WIN32
  if (is_fsync_required()) {
    segment->flush();
  }
#endif /* !_WIN32 */

#ifdef UNIV_DEBUG
  if (dblwr::Force_crash == page_id) {
    DBUG_SUICIDE();
  }
#endif /* UNIV_DEBUG */

  auto err = write_to_datafile(bpage, true, e_block);

  if (err == DB_SUCCESS) {
    fil_flush(bpage->id.space());
  } else {
    /* This block is not freed if the write_to_datafile doesn't succeed. */
    if (e_block != nullptr) {
      os_free_block(e_block);
    }
  }

  while (!s_single_segments->enqueue(segment)) {
    UT_RELAX_CPU();
  }

  /* true means we want to evict this page from the LRU list as well. */
  buf_page_io_complete(bpage, true);

  return DB_SUCCESS;
}

void Double_write::reset_file(dblwr::File &file, bool truncate) noexcept {
  auto cur_size = os_file_get_size(file.m_pfs);
  auto new_size = dblwr::File::s_n_pages * univ_page_size.physical();

  if (s_files.size() == 1) {
    new_size += SYNC_PAGE_FLUSH_SLOTS * univ_page_size.physical();
  } else if (file.is_for_lru()) {
    const auto n_bytes = (SYNC_PAGE_FLUSH_SLOTS / (s_files.size() / 2)) *
                         univ_page_size.physical();
    new_size += n_bytes;
  }

  auto pfs_file = file.m_pfs;

  if (new_size < cur_size && truncate) {
    ib::info(ER_IB_MSG_DBLWR_1306)
        << file.m_name << " size reduced to " << new_size << " bytes from "
        << cur_size << " bytes";

    auto success = os_file_truncate(file.m_name.c_str(), pfs_file, new_size);

    if (!success) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_DBLWR_1320, file.m_name.c_str());
    }

  } else if (new_size > cur_size) {
    auto err = os_file_write_zeros(pfs_file, file.m_name.c_str(),
                                   univ_page_size.physical(), cur_size,
                                   new_size - cur_size);

    if (err != DB_SUCCESS) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_DBLWR_1321, file.m_name.c_str());
    }

    ib::info(ER_IB_MSG_DBLWR_1307)
        << file.m_name << " size increased to " << new_size << " bytes "
        << "from " << cur_size << " bytes";
  }
}

void Double_write::reduced_reset_file(dblwr::File &file,
                                      uint32_t pages_per_file,
                                      uint32_t phy_size) noexcept {
  auto cur_size = os_file_get_size(file.m_pfs);
  auto new_size = pages_per_file * phy_size;
  auto pfs_file = file.m_pfs;

  if (new_size > cur_size) {
    auto err = os_file_write_zeros(pfs_file, file.m_name.c_str(), phy_size,
                                   cur_size, new_size - cur_size);

    if (err != DB_SUCCESS) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_DBLWR_1321, file.m_name.c_str());
    }

    ib::info(ER_IB_MSG_DBLWR_1307)
        << file.m_name << " size increased to " << new_size << " bytes "
        << "from " << cur_size << " bytes";
  }
}

dberr_t Double_write::init_file(dblwr::File &file, uint32_t n_pages,
                                uint32_t phy_size) noexcept {
  auto pfs_file = file.m_pfs;
  auto size = os_file_get_size(pfs_file);

  ut_ad(dblwr::File::s_n_pages > 0);

  if (size == 0) {
    auto err = os_file_write_zeros(pfs_file, file.m_name.c_str(), phy_size, 0,
                                   n_pages * phy_size);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  return DB_SUCCESS;
}

static bool is_buffer_pool_size_ok() noexcept {
  const auto min_doublewrite_size =
      ((2 * DBLWR_V1_EXTENT_SIZE + FSP_EXTENT_SIZE / 2 + 100) *
       univ_page_size.physical());

  if (buf_pool_get_curr_size() < min_doublewrite_size) {
    ib::error(ER_IB_MSG_DBLWR_1309)
        << "Buffer pool size is too small, must be at least "
        << min_doublewrite_size << " bytes";

    return false;
  }

  return true;
}

byte *Double_write::get(mtr_t *mtr) noexcept {
  const page_id_t sys_page_id(TRX_SYS_SPACE, TRX_SYS_PAGE_NO);

  auto block = buf_page_get(sys_page_id, univ_page_size, RW_X_LATCH,
                            UT_LOCATION_HERE, mtr);

  buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

  return buf_block_get_frame(block) + TRX_SYS_DBLWR_V1;
}

bool Double_write::init_v1(page_no_t &page_no1, page_no_t &page_no2) noexcept {
  mtr_t mtr;

  mtr.start();

  bool init{};
  auto doublewrite = get(&mtr);

  if (mach_read_from_4(doublewrite + DBLWR_VER) == DBLWR_V1) {
    /* The doublewrite buffer has already been created. */

    page_no1 = mach_read_from_4(doublewrite + DBLWR_V1_BLOCK1);
    page_no2 = mach_read_from_4(doublewrite + DBLWR_V1_BLOCK2);

    init = true;
  } else {
    ib::warn(ER_IB_MSG_DBLWR_1327)
        << "Legacy double write doesn't exist in the system tablespace!";
    init = false;
  }

  mtr_commit(&mtr);

  return init;
}

bool Double_write::create_v1(page_no_t &page_no1,
                             page_no_t &page_no2) noexcept {
  mtr_t mtr;

  ib::info(ER_IB_MSG_95) << "Legacy doublewrite buffer not found: creating new";

  if (!is_buffer_pool_size_ok()) {
    return false;
  }

  mtr.start();

  auto block2 = fseg_create(TRX_SYS_SPACE, TRX_SYS_PAGE_NO,
                            TRX_SYS_DBLWR_V1 + DBLWR_V1_FSEG, &mtr);

  if (block2 == nullptr) {
    ib::error(ER_IB_MSG_DBLWR_1287);
    mtr.commit();
    return false;
  }

  /* fseg_create acquires a second latch on the page,
  therefore we must declare it. */
  buf_block_dbg_add_level(block2, SYNC_NO_ORDER_CHECK);

  uint32_t prev_page_no = 0;
  byte *doublewrite = get(&mtr);
  byte *fseg_header = doublewrite + DBLWR_V1_FSEG;
  const uint32_t n_blocks = 2 * DBLWR_V1_EXTENT_SIZE + FSP_EXTENT_SIZE / 2;

  for (uint32_t i = 0; i < n_blocks; ++i) {
    auto new_block =
        fseg_alloc_free_page(fseg_header, prev_page_no + 1, FSP_UP, &mtr);

    if (new_block == nullptr) {
      ib::error(ER_IB_MSG_DBLWR_1288);
      mtr.commit();
      return false;
    }

    /* Note: We don't redo log this because we don't care. */
    mach_write_to_2(new_block->frame + FIL_PAGE_TYPE,
                    FIL_PAGE_TYPE_LEGACY_DBLWR);

    /* We read the allocated pages to the buffer pool;
    when they are written to disk in a flush, the space
    id and page number fields are also written to the
    pages. At database startup read pages from the
    doublewrite buffer, we know that if the space id and
    page number in them are the same as the page position
    in the tablespace, then the page has not been written
    to in doublewrite. */

    ut_ad(rw_lock_get_x_lock_count(&new_block->lock) == 1);

    auto page_no = new_block->page.id.page_no();

    if (i == FSP_EXTENT_SIZE / 2) {
      ut_a(page_no == FSP_EXTENT_SIZE);

      mlog_write_ulint(doublewrite + DBLWR_V1_BLOCK1, page_no, MLOG_4BYTES,
                       &mtr);

      mlog_write_ulint(doublewrite + DBLWR_V1_REPEAT + DBLWR_V1_BLOCK1, page_no,
                       MLOG_4BYTES, &mtr);

      page_no1 = page_no;

    } else if (i == FSP_EXTENT_SIZE / 2 + DBLWR_V1_EXTENT_SIZE) {
      ut_a(page_no == 2 * FSP_EXTENT_SIZE);

      mlog_write_ulint(doublewrite + DBLWR_V1_BLOCK2, page_no, MLOG_4BYTES,
                       &mtr);

      mlog_write_ulint(doublewrite + DBLWR_V1_REPEAT + DBLWR_V1_BLOCK2, page_no,
                       MLOG_4BYTES, &mtr);

      page_no2 = page_no;

    } else if (i > FSP_EXTENT_SIZE / 2) {
      ut_a(page_no == prev_page_no + 1);
    }

    if (((i + 1) & 15) == 0) {
      /* rw_locks can only be recursively x-locked
      2048 times. (on 32 bit platforms, (lint) 0 - (X_LOCK_DECR * 2049)
      is no longer a negative number, and thus lock_word becomes like a
      shared lock).  For 4k page size this loop will lock the fseg header
      too many times. Since this code is not done while any other threads
      are active, restart the MTR occasionally. */

      mtr.commit();

      mtr.start();

      doublewrite = get(&mtr);

      fseg_header = doublewrite + DBLWR_V1_FSEG;
    }

    prev_page_no = page_no;
  }

  auto ptr = doublewrite + DBLWR_VER;

  mlog_write_ulint(ptr, DBLWR_V1, MLOG_4BYTES, &mtr);

  ptr += DBLWR_V1_REPEAT;

  mlog_write_ulint(ptr, DBLWR_V1, MLOG_4BYTES, &mtr);

  ptr = doublewrite + DBLWR_V1_SPACE_ID_STORED;

  mlog_write_ulint(ptr, DBLWR_V1_SPACE_ID_STORED_N, MLOG_4BYTES, &mtr);

  mtr.commit();

  /* Flush the modified pages to disk and make a checkpoint. */
  log_make_latest_checkpoint();

  /* Remove doublewrite pages from the LRU list. */
  buf_pool_invalidate();

  ib::info(ER_IB_MSG_99) << "Legacy atomic write buffer created";

  return true;
}

dberr_t Double_write::load(dblwr::File &file, recv::Pages *pages) noexcept {
  os_offset_t size = os_file_get_size(file.m_pfs);

  if (size == 0) {
    /* Double write buffer is empty. */
    ib::info(ER_IB_MSG_DBLWR_1285, file.m_name.c_str());

    return DB_SUCCESS;
  }

  if ((size % univ_page_size.physical())) {
    ib::warn(ER_IB_MSG_DBLWR_1319, file.m_name.c_str(), (ulint)size,
             (ulint)univ_page_size.physical());
  }

  const uint32_t n_pages = size / univ_page_size.physical();

  Buffer buffer{n_pages};
  IORequest read_request(IORequest::READ);

  read_request.disable_compression();

  auto err = os_file_read(read_request, file.m_name.c_str(), file.m_pfs,
                          buffer.begin(), 0, buffer.capacity());

  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_DBLWR_1301, ut_strerr(err));

    return err;
  }

  auto page = buffer.begin();

  for (uint32_t i = 0; i < n_pages; ++i) {
    pages->add(i, page, univ_page_size.physical());
    page += univ_page_size.physical();
  }

  return DB_SUCCESS;
}

/** Reduced doublewrite file deserializer. Used during crash recovery. */
class Reduced_batch_deserializer {
 public:
  /** Constructor
  @param[in]   buf     Buffer to hold the Reduced dblwr pages
  @param[in]   n_pages Number of reduced dblwr pages */
  explicit Reduced_batch_deserializer(Buffer *buf, uint32_t n_pages)
      : m_buf(buf), m_n_pages(n_pages) {}

  /** Deserialize page and call Functor f for each page_entry found
  from reduced dblwr page
  @param[in]   f       Functor to process page entry from dblwr page
  @return DB_SUCCESS on success, others of checksum or parsing failures */
  template <typename F>
  dberr_t deserialize(F &f) {
    auto page = m_buf->begin();
    for (uint32_t i = 0; i < m_n_pages; ++i) {
      if (is_zeroes(page)) {
        page += REDUCED_BATCH_PAGE_SIZE;
        continue;
      }
      dberr_t err = parse_page(page, f);
      if (err != DB_SUCCESS) {
        ib::error(ER_REDUCED_DBLWR_FILE_CORRUPTED, i);
        return (err);
      }
      page += REDUCED_BATCH_PAGE_SIZE;
    }
    return (DB_SUCCESS);
  }

 private:
  /** Parse reduced dblwr batch page header
  @param[in]   page            Page to parse
  @param[in]   data_len        length of data in page
  @return DB_SUCCESS on success, others on failure */
  dberr_t parse_header(const byte *page, uint16_t *data_len) noexcept {
    //    uint32_t batch_id = mach_read_from_4(page + RB_OFF_BATCH_ID);
    uint32_t checksum = mach_read_from_4(page + RB_OFF_CHECKSUM);
    *data_len = mach_read_from_2(page + RB_OFF_DATA_LEN);
    //   buf_flush_t flush_type =
    //   static_cast<buf_flush_t>(page[RB_OFF_BATCH_TYPE]);

    if (*data_len == 0) {
      return (DB_CORRUPTION);
    }
    if (*data_len % REDUCED_ENTRY_SIZE != 0) {
      return (DB_CORRUPTION);
    }

    uint32_t calc_checksum = ut_crc32(page + REDUCED_HEADER_SIZE, *data_len);
    if (checksum != calc_checksum) {
      return (DB_CORRUPTION);
    }
    return (DB_SUCCESS);
  }

  /* Utility function to parse page
  @param[in]   page    reduced dblwr batch page
  @param[in]   f       Callback function that process page entries
  @return DB_SUCCESS on success */
  template <typename F>
  dberr_t parse_page(const byte *page, F &f) noexcept {
    uint16_t data_len{};

    dberr_t err = parse_header(page, &data_len);
    if (err != DB_SUCCESS) {
      return (err);
    }

    parse_page_data(page, data_len, f);
    return (DB_SUCCESS);
  }

  /** Utility function to parse page data
  @param[in]   page            reduced dblwr batch page
  @param[in]   data_len        length of data in page
  @param[in]   f               Callback function that process page entries */
  template <typename F>
  void parse_page_data(const byte *page, uint16_t data_len, F &f) noexcept {
    const byte *page_data = page + REDUCED_HEADER_SIZE;
#ifdef UNIV_DEBUG
    const byte *page_start [[maybe_unused]] = page + REDUCED_HEADER_SIZE;
#endif /* UNIV_DEBUG */
    const uint32_t expected_entries = data_len / REDUCED_ENTRY_SIZE;
    for (uint32_t entry = 1; entry <= expected_entries; ++entry) {
      space_id_t space_id = mach_read_from_4(page_data);
      page_data += 4;
      page_no_t page_num = mach_read_from_4(page_data);
      page_data += 4;
      lsn_t lsn = mach_read_from_8(page_data);
      page_data += 8;
      Page_entry pe(space_id, page_num, lsn);
      f(pe);
    }

    ut_ad(static_cast<uint32_t>(page_data - page_start) ==
          (expected_entries * REDUCED_ENTRY_SIZE));
  }

  /** @return true if dblwr page is an all-zero page
  @param[in]   page    dblwr page in batch file (.bdblwr) */
  bool is_zeroes(const byte *page) {
    for (ulint i = 0; i < REDUCED_BATCH_PAGE_SIZE; i++) {
      if (page[i] != 0) {
        return (false);
      }
    }
    return (true);
  }

 private:
  /** Temporary buffer to hold Reduced dblwr pages */
  Buffer *m_buf;
  /** Number of reduced dblwr pages */
  uint32_t m_n_pages;
};

dberr_t Double_write::load_reduced_batch(dblwr::File &file,
                                         recv::Pages *pages) noexcept {
  os_offset_t size = os_file_get_size(file.m_pfs);

  if (srv_read_only_mode) {
    ib::info() << "Skipping doublewrite buffer processing due to "
                  "InnoDB running in read only mode";
    return (DB_SUCCESS);
  }

  if (size == 0) {
    /* Double write buffer is empty. */
    ib::info(ER_IB_MSG_DBLWR_1285, file.m_name.c_str());

    return DB_SUCCESS;
  }

  if ((size % REDUCED_BATCH_PAGE_SIZE) != 0) {
    ib::warn(ER_IB_MSG_DBLWR_1319, file.m_name.c_str(), (ulint)size,
             (ulint)REDUCED_BATCH_PAGE_SIZE);
  }

  const uint32_t n_pages = size / REDUCED_BATCH_PAGE_SIZE;
  Buffer buffer(n_pages, REDUCED_BATCH_PAGE_SIZE);
  IORequest read_request(IORequest::READ);

  read_request.disable_compression();

  auto err = os_file_read(read_request, file.m_name.c_str(), file.m_pfs,
                          buffer.begin(), 0, buffer.capacity());

  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_DBLWR_1301, ut_strerr(err));

    return err;
  }

  auto page_entry_processor = [&](Page_entry &pe) { pages->add_entry(pe); };

  Reduced_batch_deserializer rbd(&buffer, n_pages);
  err = rbd.deserialize(page_entry_processor);

  return (err);
}

uint16_t Double_write::write_dblwr_pages(buf_flush_t flush_type) noexcept {
  ut_ad(mutex_own(&m_mutex));
  ut_a(!m_buffer.empty());

  Batch_segment *batch_segment{};

  auto segments = flush_type == BUF_FLUSH_LRU ? s_LRU_batch_segments
                                              : s_flush_list_batch_segments;

  while (!segments->dequeue(batch_segment)) {
    std::this_thread::yield();
  }

  batch_segment->start(this);

  batch_segment->write(m_buffer);

  m_bytes_written += m_buffer.size();

  m_buffer.clear();

#ifndef _WIN32
  if (is_fsync_required()) {
    batch_segment->flush();
  }
#endif /* !_WIN32 */

  batch_segment->set_batch_size(m_buf_pages.size());

  return batch_segment->id();
}

void Double_write::write_data_pages(buf_flush_t flush_type,
                                    uint16_t batch_id) noexcept {
  ut_ad(mutex_own(&m_mutex));

  for (uint32_t i = 0; i < m_buf_pages.size(); ++i) {
    const auto bpage = std::get<0>(m_buf_pages.m_pages[i]);

    ut_d(auto page_id = bpage->id);

    bpage->set_dblwr_batch_id(batch_id);

    ut_d(bpage->take_io_responsibility());
    auto err =
        write_to_datafile(bpage, false, std::get<1>(m_buf_pages.m_pages[i]));

    if (err == DB_PAGE_IS_STALE || err == DB_TABLESPACE_DELETED) {
      /* For async operation, if space is deleted, fil_io already
      does buf_page_io_complete and returns DB_TABLESPACE_DELETED.
      buf_page_free_stale_during_write() asserts if not IO fixed
      and does similar things as buf_page_io_complete(). This is a
      temp fix to address this situation. Ideally we should handle
      these errors in single place possibly by one function. */
      if (bpage->was_io_fixed()) {
        write_complete(bpage, flush_type);
        buf_page_free_stale_during_write(
            bpage, buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);
      }

      const file::Block *block = std::get<1>(m_buf_pages.m_pages[i]);
      if (block != nullptr) {
        os_free_block(const_cast<file::Block *>(block));
      }
    } else {
      ut_a(err == DB_SUCCESS);
    }
    /* We don't hold io_responsibility here no matter which path through ifs and
    elses we've got here, but we can't assert:
      ut_ad(!bpage->current_thread_has_io_responsibility());
    because bpage could be freed by the time we got here. */

#ifdef UNIV_DEBUG
    if (dblwr::Force_crash == page_id) {
      DBUG_SUICIDE();
    }
#endif /* UNIV_DEBUG */
  }

  srv_stats.dblwr_writes.inc();

  m_buf_pages.clear();

  os_aio_simulated_wake_handler_threads();
}

void Double_write::write_pages(buf_flush_t flush_type) noexcept {
  ut_ad(mutex_own(&m_mutex));
  ut_a(!m_buffer.empty());

  const uint16_t batch_id = write_dblwr_pages(flush_type);
  write_data_pages(flush_type, batch_id);
}

dberr_t Double_write::create_batch_segments(
    uint32_t segments_per_file) noexcept {
  const uint32_t n_segments = segments_per_file * s_files.size();

  /* Maximum size of the queue needs to be a power of 2. */
  const auto max_queue_size =
      std::max(ulint{2}, ut_2_power_up((n_segments + 1)));

  ut_a(s_LRU_batch_segments == nullptr);

  s_LRU_batch_segments =
      ut::new_withkey<Batch_segments>(UT_NEW_THIS_FILE_PSI_KEY, max_queue_size);

  if (s_LRU_batch_segments == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  ut_a(s_flush_list_batch_segments == nullptr);

  s_flush_list_batch_segments =
      ut::new_withkey<Batch_segments>(UT_NEW_THIS_FILE_PSI_KEY, max_queue_size);

  if (s_flush_list_batch_segments == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  const uint32_t total_pages = segments_per_file * dblwr::n_pages;

  uint16_t id{};

  for (auto &file : s_files) {
    for (uint32_t i = 0; i < total_pages; i += dblwr::n_pages, ++id) {
      auto s = ut::new_withkey<Batch_segment>(UT_NEW_THIS_FILE_PSI_KEY, id,
                                              file, i, dblwr::n_pages);

      if (s == nullptr) {
        return DB_OUT_OF_MEMORY;
      }

      Batch_segments *segments{};

      if (s_files.size() > 1) {
        segments = file.is_for_lru() ? s_LRU_batch_segments
                                     : s_flush_list_batch_segments;
      } else {
        segments =
            is_odd(id) ? s_LRU_batch_segments : s_flush_list_batch_segments;
      }

      auto success = segments->enqueue(s);
      ut_a(success);
      s_segments.push_back(s);
      s_regular_last_batch_id = id;
    }
  }

  return DB_SUCCESS;
}

dberr_t Double_write::create_reduced_batch_segments() noexcept {
  ut_ad(Double_write::s_n_instances >= 4);

  /* Maximum size of the queue needs to be a power of 2. */
  const auto max_queue_size =
      std::max(ulint{2}, ut_2_power_up(Double_write::s_n_instances / 2));

  ut_a(s_r_LRU_batch_segments == nullptr);

  s_r_LRU_batch_segments =
      ut::new_withkey<Batch_segments>(UT_NEW_THIS_FILE_PSI_KEY, max_queue_size);

  if (s_r_LRU_batch_segments == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  ut_a(s_r_flush_list_batch_segments == nullptr);

  s_r_flush_list_batch_segments =
      ut::new_withkey<Batch_segments>(UT_NEW_THIS_FILE_PSI_KEY, max_queue_size);

  if (s_r_flush_list_batch_segments == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  const uint32_t total_pages = Double_write::s_n_instances;
  const uint32_t pages_per_segment = 1;

  /* Batch_ids for new segments should start after old batch ids. */
  uint16_t id = Double_write::s_segments.size();

  /* The number of pages in the reduced dblwr file is equal to the number of
  instances. Use half of it for LRU batch segments and the rest for flush list
  batch segments. */
  const auto lru_segs = Double_write::s_n_instances / 2;

  /* Reduced Batch file */
  auto &file = Double_write::s_r_files[0];
  for (uint32_t i = 0; i < total_pages; ++i, ++id) {
    auto s = ut::new_withkey<Batch_segment>(UT_NEW_THIS_FILE_PSI_KEY, id, file,
                                            REDUCED_BATCH_PAGE_SIZE, i,
                                            pages_per_segment);

    if (s == nullptr) {
      return DB_OUT_OF_MEMORY;
    }

    Batch_segments *segments =
        (i < lru_segs) ? s_r_LRU_batch_segments : s_r_flush_list_batch_segments;
    auto success = segments->enqueue(s);
    ut_a(success);
    s_segments.push_back(s);
  }
  return DB_SUCCESS;
}

dberr_t Double_write::create_single_segments() noexcept {
  ut_a(s_single_segments == nullptr);

  /* This needs to be a power of 2. */
  const auto max_queue_size =
      std::max(ulint{2}, ut_2_power_up(SYNC_PAGE_FLUSH_SLOTS));

  s_single_segments =
      ut::new_withkey<Segments>(UT_NEW_THIS_FILE_PSI_KEY, max_queue_size);

  if (s_single_segments == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  uint32_t n_pages{};

  if (s_files.size() == 1) {
    n_pages = SYNC_PAGE_FLUSH_SLOTS;
  } else {
    n_pages = SYNC_PAGE_FLUSH_SLOTS / (s_files.size() / 2);
  }

  for (auto &file : s_files) {
    if (!file.is_for_lru() && s_files.size() > 1) {
      /* Skip the flush list files. */
      continue;
    }
    const auto start = dblwr::File::s_n_pages;

    for (uint32_t i = start; i < start + n_pages; ++i) {
      auto s = ut::new_withkey<Segment>(UT_NEW_THIS_FILE_PSI_KEY, file, i, 1UL);

      if (s == nullptr) {
        return DB_OUT_OF_MEMORY;
      }

      auto success = s_single_segments->enqueue(s);
      ut_a(success);
    }
  }

  return DB_SUCCESS;
}

file::Block *dblwr::get_encrypted_frame(buf_page_t *bpage) noexcept {
  space_id_t space_id = bpage->space();
  page_no_t page_no = bpage->page_no();

  if (page_no == 0) {
    /* The first page of any tablespace is never encrypted.
    So return early. */
    return nullptr;
  }

  if (fsp_is_undo_tablespace(space_id) && !srv_undo_log_encrypt) {
    /* It is an undo tablespace and undo encryption is not enabled. */
    return nullptr;
  }

  fil_space_t *space = bpage->get_space();
  if (space->encryption_op_in_progress == Encryption::Progress::DECRYPTION ||
      !space->is_encrypted()) {
    return nullptr;
  }

  if (!space->can_encrypt()) {
    /* Encryption key information is not available. */
    return nullptr;
  }

  IORequest type(IORequest::WRITE);
  void *frame{};
  uint32_t len{};

  fil_node_t *node = space->get_file_node(&page_no);
  type.block_size(node->block_size);

  Double_write::prepare(bpage, &frame, &len);

  ulint n = len;

  file::Block *compressed_block{};

  /* Transparent page compression (TPC) is disabled if punch hole is not
  supported. A similar check is done in Fil_shard::do_io(). */
  const bool do_compression =
      space->is_compressed() && !bpage->size.is_compressed() &&
      IORequest::is_punch_hole_supported() && node->punch_hole;

  if (do_compression) {
    /* @note Compression needs to be done before encryption. */

    /* The page size must be a multiple of the OS punch hole size. */
    ut_ad(n % type.block_size() == 0);

    type.compression_algorithm(space->compression_type);
    compressed_block = os_file_compress_page(type, frame, &n);
  }

  type.get_encryption_info().set(space->m_encryption_metadata);
  auto e_block = os_file_encrypt_page(type, frame, n);

  if (compressed_block != nullptr) {
    file::Block::free(compressed_block);
  }

  return e_block;
}

dberr_t dblwr::write(buf_flush_t flush_type, buf_page_t *bpage,
                     bool sync) noexcept {
  dberr_t err;
  const space_id_t space_id = bpage->id.space();

  ut_ad(bpage->current_thread_has_io_responsibility());
  /* This is not required for correctness, but it aborts the processing early.
   */
  if (bpage->was_stale()) {
    /* Disable batch completion in write_complete(). */
    bpage->set_dblwr_batch_id(std::numeric_limits<uint16_t>::max());
    buf_page_free_stale_during_write(
        bpage, buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);
    /* We don't hold io_responsibility here no matter which path through ifs and
    elses we've got here, but we can't assert:
      ut_ad(!bpage->current_thread_has_io_responsibility());
    because bpage could be freed by the time we got here. */
    return DB_SUCCESS;
  }

  if (srv_read_only_mode || fsp_is_system_temporary(space_id) ||
      !dblwr::is_enabled() || Double_write::s_instances == nullptr ||
      mtr_t::s_logging.dblwr_disabled()) {
    /* Skip the double-write buffer since it is not needed. Temporary
    tablespaces are never recovered, therefore we don't care about
    torn writes. */
    bpage->set_dblwr_batch_id(std::numeric_limits<uint16_t>::max());
    err = Double_write::write_to_datafile(bpage, sync, nullptr);
    if (err == DB_PAGE_IS_STALE || err == DB_TABLESPACE_DELETED) {
      if (bpage->was_io_fixed()) {
        buf_page_free_stale_during_write(
            bpage, buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);
      }
      err = DB_SUCCESS;
    } else if (sync) {
      ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_SINGLE_PAGE);

      if (err == DB_SUCCESS) {
        fil_flush(space_id);
      }
      /* true means we want to evict this page from the LRU list as well. */
      buf_page_io_complete(bpage, true);
    }

  } else {
    ut_d(auto page_id = bpage->id);

    /* Encrypt the page here, so that the same encrypted contents are written
    to the dblwr file and the data file. */
    file::Block *e_block = dblwr::get_encrypted_frame(bpage);

    if (!sync && flush_type != BUF_FLUSH_SINGLE_PAGE) {
      MONITOR_INC(MONITOR_DBLWR_ASYNC_REQUESTS);

      ut_d(bpage->release_io_responsibility());
      Double_write::submit(flush_type, bpage, e_block);
      err = DB_SUCCESS;
#ifdef UNIV_DEBUG
      if (dblwr::Force_crash == page_id) {
        force_flush(flush_type, buf_pool_index(buf_pool_from_bpage(bpage)));
      }
#endif /* UNIV_DEBUG */
    } else {
      MONITOR_INC(MONITOR_DBLWR_SYNC_REQUESTS);
      /* Disable batch completion in write_complete(). */
      bpage->set_dblwr_batch_id(std::numeric_limits<uint16_t>::max());
      err = Double_write::sync_page_flush(bpage, e_block);
    }
  }
  /* We don't hold io_responsibility here no matter which path through ifs and
  elses we've got here, but we can't assert:
    ut_ad(!bpage->current_thread_has_io_responsibility());
  because bpage could be freed by the time we got here. */
  return err;
}

bool Double_write::is_reduced_batch_id(uint32_t batch_id) {
  ut_ad(s_regular_last_batch_id != 0);
  return (batch_id > s_regular_last_batch_id);
}

void Double_write::write_complete(buf_page_t *bpage,
                                  buf_flush_t flush_type) noexcept {
  if (s_instances == nullptr) {
    /* Not initialized yet. */
    return;
  }

  const auto batch_id = bpage->get_dblwr_batch_id();

  switch (flush_type) {
    case BUF_FLUSH_LRU:
    case BUF_FLUSH_LIST:
    case BUF_FLUSH_SINGLE_PAGE:
      if (batch_id != std::numeric_limits<uint16_t>::max()) {
        ut_ad(batch_id < s_segments.size());
        auto batch_segment = s_segments[batch_id];

        if (batch_segment->write_complete()) {
          batch_segment->completed();

          srv_stats.dblwr_pages_written.add(batch_segment->batch_size());

          batch_segment->reset();

          Batch_segments *segments{nullptr};

          if (is_reduced_batch_id(batch_id)) {
            segments = (flush_type == BUF_FLUSH_LRU)
                           ? Double_write::s_r_LRU_batch_segments
                           : Double_write::s_r_flush_list_batch_segments;
          } else {
            segments = (flush_type == BUF_FLUSH_LRU)
                           ? Double_write::s_LRU_batch_segments
                           : Double_write::s_flush_list_batch_segments;
          }

          fil_flush_file_spaces();

          while (!segments->enqueue(batch_segment)) {
            std::this_thread::yield();
          }
        }
      }
      bpage->set_dblwr_batch_id(std::numeric_limits<uint16_t>::max());
      break;

    case BUF_FLUSH_N_TYPES:
      ut_error;
  }
}

void dblwr::write_complete(buf_page_t *bpage, buf_flush_t flush_type) noexcept {
  Double_write::write_complete(bpage, flush_type);
}

void dblwr::recv::recover(recv::Pages *pages, fil_space_t *space) noexcept {
#ifndef UNIV_HOTBACKUP
  pages->recover(space);
#endif /* UNIV_HOTBACKUP */
}

/** Create the file and or open it if it exists.
@param[in] dir_name             Directory where to create the file.
@param[in] id                   Instance ID.
@param[out] file                File handle.
@param[in] file_type            The file type.
@param[in] extension            .dblwr/.bdblwr
@return DB_SUCCESS if all went well. */
static dberr_t dblwr_file_open(const std::string &dir_name, int id,
                               dblwr::File &file, ulint file_type,
                               ib_file_suffix extension = DWR) noexcept {
  bool dir_exists;
  bool file_exists;
  os_file_type_t type;
  std::string dir(dir_name);

  Fil_path::normalize(dir);

  os_file_status(dir.c_str(), &dir_exists, &type);

  switch (type) {
    case OS_FILE_TYPE_DIR:
      /* This is an existing directory. */
      break;
    case OS_FILE_TYPE_MISSING:
      /* This path is missing but otherwise usable. It will be created. */
      ut_ad(!dir_exists);
      break;
    case OS_FILE_TYPE_LINK:
    case OS_FILE_TYPE_FILE:
    case OS_FILE_TYPE_BLOCK:
    case OS_FILE_TYPE_UNKNOWN:
    case OS_FILE_TYPE_FAILED:
    case OS_FILE_PERMISSION_ERROR:
    case OS_FILE_TYPE_NAME_TOO_LONG:

      ib::error(ER_IB_MSG_DBLWR_1290, dir_name.c_str());

      return DB_WRONG_FILE_NAME;
  }

  file.m_id = id;

  file.m_name = std::string(dir_name) + OS_PATH_SEPARATOR + "#ib_";

  file.m_name += std::to_string(srv_page_size) + "_" + std::to_string(id);

  file.m_name += dot_ext[extension];

  uint32_t mode;
  if (dir_exists) {
    os_file_status(file.m_name.c_str(), &file_exists, &type);

    if (type == OS_FILE_TYPE_FILE) {
      mode = OS_FILE_OPEN_RETRY;
    } else if (type == OS_FILE_TYPE_MISSING) {
      mode = OS_FILE_CREATE;
    } else {
      ib::error(ER_IB_MSG_BAD_DBLWR_FILE_NAME, file.m_name.c_str());

      return DB_CANNOT_OPEN_FILE;
    }
  } else {
    auto err = os_file_create_subdirs_if_needed(file.m_name.c_str());
    if (err != DB_SUCCESS) {
      return err;
    }

    mode = OS_FILE_CREATE;
  }

  if (mode == OS_FILE_CREATE && id >= (int)Double_write::s_n_instances) {
    /* Don't create files if not configured by the user. */
    return DB_NOT_FOUND;
  }

  bool success;
  file.m_pfs =
      os_file_create(innodb_dblwr_file_key, file.m_name.c_str(), mode,
                     OS_FILE_NORMAL, file_type, srv_read_only_mode, &success);

  if (!success) {
    ib::error(ER_IB_MSG_DBLWR_1293, file.m_name.c_str());
    return DB_IO_ERROR;
  } else {
    ib::info(ER_IB_MSG_DBLWR_1286, file.m_name.c_str());
  }

  return DB_SUCCESS;
}

dberr_t dblwr::reduced_open() noexcept {
  ut_a(Double_write::s_r_files.empty());

  /* The number of files for reduced dblwr is always one. */
  Double_write::s_r_files.resize(1);

  dberr_t err{DB_SUCCESS};

  /* Create the batch file */
  auto &file = Double_write::s_r_files[0];

  uint32_t pages_per_file = Double_write::s_n_instances;
  ib_file_suffix extension{BWR};

  err = dblwr_file_open(dblwr::dir, 0, file, OS_DBLWR_FILE, extension);

  if (err != DB_SUCCESS) {
    return (err);
  }
  const uint32_t phy_size = REDUCED_BATCH_PAGE_SIZE;

  err = Double_write::init_file(file, pages_per_file, phy_size);

  if (err != DB_SUCCESS) {
    return (err);
  }

  auto file_size = os_file_get_size(file.m_pfs);

  if (file_size == 0 || (file_size % phy_size)) {
    ib::warn(ER_IB_MSG_DBLWR_1322, file.m_name.c_str(), (ulint)file_size,
             (ulint)phy_size);
  }

  Double_write::reduced_reset_file(file, pages_per_file, phy_size);

  return (DB_SUCCESS);
}

dberr_t dblwr::open() noexcept {
  ut_a(!dblwr::dir.empty());
  ut_a(Double_write::s_files.empty());
  ut_a(Double_write::s_n_instances == 0);

  /* Separate instances for LRU and FLUSH list write requests. */
  Double_write::s_n_instances = std::max(4UL, srv_buf_pool_instances * 2);

  /* Batch segments per dblwr file. */
  uint32_t segments_per_file{};

  if (dblwr::n_files == 0) {
    dblwr::n_files = 2;
  }

  ib::info(ER_IB_MSG_DBLWR_1324)
      << "Double write buffer files: " << dblwr::n_files;

  if (dblwr::n_pages == 0) {
    dblwr::n_pages = srv_n_write_io_threads;
  }

  ib::info(ER_IB_MSG_DBLWR_1323)
      << "Double write buffer pages per instance: " << dblwr::n_pages;

  if (Double_write::s_n_instances < dblwr::n_files) {
    segments_per_file = 1;
    Double_write::s_files.resize(Double_write::s_n_instances);
  } else {
    Double_write::s_files.resize(dblwr::n_files);
    segments_per_file = (Double_write::s_n_instances / dblwr::n_files) + 1;
  }

  dberr_t err{DB_SUCCESS};

  ut_ad(dblwr::File::s_n_pages == 0);
  dblwr::File::s_n_pages = dblwr::n_pages * segments_per_file;

  const auto first = &Double_write::s_files[0];

  /* Create the files (if required) and make them the right size. */
  for (auto &file : Double_write::s_files) {
    err = dblwr_file_open(dblwr::dir, &file - first, file, OS_DBLWR_FILE);

    if (err != DB_SUCCESS) {
      break;
    }

    auto pages_per_file = dblwr::n_pages * segments_per_file;

    if (Double_write::s_files.size() == 1) {
      pages_per_file += SYNC_PAGE_FLUSH_SLOTS;
    } else if ((file.m_id & 1)) {
      pages_per_file +=
          SYNC_PAGE_FLUSH_SLOTS / (Double_write::s_files.size() / 2);
    }

    err = Double_write::init_file(file, pages_per_file);

    if (err != DB_SUCCESS) {
      break;
    }

    auto file_size = os_file_get_size(file.m_pfs);

    if (file_size == 0 || (file_size % univ_page_size.physical())) {
      ib::warn(ER_IB_MSG_DBLWR_1322, file.m_name.c_str(), (ulint)file_size,
               (ulint)univ_page_size.physical());
    }

    /* Truncate the size after recovery: false. */
    Double_write::reset_file(file, false);
  }

  /* Create the segments that for LRU and FLUSH list batches writes */
  if (err == DB_SUCCESS) {
    err = Double_write::create_batch_segments(segments_per_file);
  }

  /* Create the segments for the single page flushes. */
  if (err == DB_SUCCESS) {
    err = Double_write::create_single_segments();
  }

  if (err == DB_SUCCESS) {
    err = Double_write::create_v2();
  } else {
    Double_write::shutdown();
  }

  if (err != DB_SUCCESS) {
    return (err);
  }

  if (!dblwr::is_reduced()) {
    return (DB_SUCCESS);
  }

  if (err == DB_SUCCESS) {
    err = dblwr::enable_reduced();
  }

  return err;
}

dberr_t dblwr::enable_reduced() noexcept {
  if (is_reduced_inited) {
    return (DB_SUCCESS);
  }

  dberr_t err = dblwr::reduced_open();

  /* Create the segments that for LRU and FLUSH list batches writes */
  if (err == DB_SUCCESS) {
    err = Double_write::create_reduced_batch_segments();
  }

  if (err == DB_SUCCESS) {
    err = Double_write::create_reduced();
  }

  if (err == DB_SUCCESS) {
    is_reduced_inited = true;
  }

  return (err);
}

void dblwr::close() noexcept { Double_write::shutdown(); }

void dblwr::set() {
#ifndef UNIV_HOTBACKUP
  Double_write::toggle(dblwr::g_mode);
#endif /* !UNIV_HOTBACKUP */
}

void dblwr::reset_files() noexcept { Double_write::reset_files(); }

dberr_t dblwr::v1::init() noexcept {
  if (!Double_write::init_v1(LEGACY_PAGE1, LEGACY_PAGE2)) {
    return DB_V1_DBLWR_INIT_FAILED;
  }

  return DB_SUCCESS;
}

dberr_t dblwr::v1::create() noexcept {
  if (!Double_write::create_v1(LEGACY_PAGE1, LEGACY_PAGE2)) {
    return DB_V1_DBLWR_CREATE_FAILED;
  }

  return DB_SUCCESS;
}

bool dblwr::v1::is_inside(page_no_t page_no) noexcept {
  if (LEGACY_PAGE1 == 0) {
    ut_a(LEGACY_PAGE2 == 0);
    /* We don't want our own reads being checked here during initialisation. */
    return false;
  }
  if (page_no >= LEGACY_PAGE1 &&
      page_no < LEGACY_PAGE1 + DBLWR_V1_EXTENT_SIZE) {
    return true;
  }

  if (page_no >= LEGACY_PAGE2 &&
      page_no < LEGACY_PAGE2 + DBLWR_V1_EXTENT_SIZE) {
    return true;
  }

  return false;
}

/** Check if the dblwr page is corrupted.
@param[in]  page  the dblwr page.
@param[in]  space  tablespace to which the page belongs.
@param[in]  page_no  page_no within the actual tablespace.
@param[out]  err     error code to check if decryption or decompression failed.
@return true if dblwr page is corrupted, false otherwise. */
static bool is_dblwr_page_corrupted(byte *page, fil_space_t *space,
                                    page_no_t page_no, dberr_t *err) noexcept {
  const page_size_t page_size(space->flags);
  const bool is_checksum_disabled = fsp_is_checksum_disabled(space->id);
  bool corrupted = false;

  BlockReporter dblwr_page(true, page, page_size, is_checksum_disabled);

  if (dblwr_page.is_encrypted()) {
    Encryption en;
    IORequest req_type;
    size_t z_page_size;

    en.set(space->m_encryption_metadata);
    fil_node_t *node = space->get_file_node(&page_no);
    req_type.block_size(node->block_size);

    page_type_t page_type = fil_page_get_type(page);
    ut_ad(fil_is_page_type_valid(page_type));

    if (page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED) {
      uint16_t z_len = mach_read_from_2(page + FIL_PAGE_COMPRESS_SIZE_V1);
      z_page_size = z_len + FIL_PAGE_DATA;

      /* @note The block size needs to be the same when the page was compressed
      and encrypted. */
      z_page_size = ut_calc_align(z_page_size, req_type.block_size());
    } else {
      z_page_size = page_size.physical();
    }

    *err = en.decrypt(req_type, page, z_page_size, nullptr, 0);
    if (*err != DB_SUCCESS) {
      /* Could not decrypt.  Consider it corrupted. */
      corrupted = true;

      if (*err == DB_IO_DECRYPT_FAIL) {
        std::ostringstream out;
        out << "space_id=" << space->id << ", page_no=" << page_no
            << ", page_size=" << z_page_size << ", space_name=" << space->name;
        ib::warn(ER_IB_DBLWR_DECRYPT_FAILED, out.str().c_str());

        if (en.is_none()) {
          std::ostringstream out;
          out << "space_id=" << space->id << ", space_name=" << space->name;
          ib::warn(ER_IB_DBLWR_KEY_MISSING, out.str().c_str());
        }
      }

    } else {
      /* Check if the page is compressed. */
      page_type_t page_type = fil_page_get_type(page);
      ut_ad(fil_is_page_type_valid(page_type));

      if (page_type == FIL_PAGE_COMPRESSED) {
        *err = os_file_decompress_page(true, page, nullptr, 0);

        if (*err != DB_SUCCESS) {
          /* Could not decompress.  Consider it corrupted. */
          size_t orig_size = mach_read_from_2(page + FIL_PAGE_ORIGINAL_SIZE_V1);
          ib::error(ER_IB_DBLWR_DECOMPRESS_FAILED, *err, orig_size);
          corrupted = true;
        }
      }
    }
  }

  if (!corrupted) {
    BlockReporter check(true, page, page_size, is_checksum_disabled);
    corrupted = check.is_corrupted();
  }

  return (corrupted);
}

/** Recover a page from the doublewrite buffer.
@param[in]      dblwr_page_no         Page number if the doublewrite buffer
@param[in]      space                       Tablespace the page belongs to
@param[in]      page_no                   Page number in the tablespace
@param[in]      page                        Data to write to <space, page_no>
@return true if page was restored to the tablespace */
bool dblwr::recv::Pages::dblwr_recover_page(page_no_t dblwr_page_no,
                                            fil_space_t *space,
                                            page_no_t page_no,
                                            byte *page) noexcept {
  /* For cloned database double write pages should be ignored. However,
  given the control flow, we read the pages in anyway but don't recover
  from the pages we read in. */
  ut_a(!recv_sys->is_cloned_db);

  Buffer buffer{1};

  if (page_no >= space->size) {
    /* Do not report the warning if the tablespace is going to be truncated. */
    if (!undo::is_active(space->id)) {
      ib::warn(ER_IB_MSG_DBLWR_1313)
          << "Page# " << dblwr_page_no
          << " stored in the doublewrite file is"
             " not within data file space bounds "
          << space->size << " bytes:  page : " << page_id_t(space->id, page_no);
    }

    return false;
  }

  const page_size_t page_size(space->flags);
  const page_id_t page_id(space->id, page_no);

  /* We want to ensure that for partial reads the
  unread portion of the page is NUL. */
  memset(buffer.begin(), 0x0, page_size.physical());

  IORequest request;

  request.dblwr();

  /* Read in the page from the data file to compare. */
  auto err = fil_io(request, true, page_id, page_size, 0, page_size.physical(),
                    buffer.begin(), nullptr);

  if (err != DB_SUCCESS) {
    ib::warn(ER_IB_MSG_DBLWR_1314)
        << "Double write file recovery: " << page_id << " read failed with "
        << "error: " << ut_strerr(err);
  }

  /* Is the page read from the data file corrupt? */
  BlockReporter data_file_page(true, buffer.begin(), page_size,
                               fsp_is_checksum_disabled(space->id));

  if (data_file_page.is_corrupted()) {
    ib::info(ER_IB_MSG_DBLWR_1315) << "Database page corruption or"
                                   << " a failed file read of page " << page_id
                                   << ". Trying to recover it from the"
                                   << " doublewrite file.";

    dberr_t dblwr_err;

    const bool dblwr_corrupted =
        is_dblwr_page_corrupted(page, space, page_no, &dblwr_err);

    if (dblwr_corrupted) {
      std::ostringstream out;

      out << "Dumping the data file page (page_id=" << page_id << "):";
      ib::error(ER_IB_MSG_DBLWR_1304, out.str().c_str());

      buf_page_print(buffer.begin(), page_size, BUF_PAGE_PRINT_NO_CRASH);

      out.str("");
      out << "Dumping the DBLWR page (dblwr_page_no=" << dblwr_page_no << "):";
      ib::error(ER_IB_MSG_DBLWR_1295, out.str().c_str());

      buf_page_print(page, page_size, BUF_PAGE_PRINT_NO_CRASH);

      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_DBLWR_1306);
    }

  } else {
    bool data_page_zeroes = buf_page_is_zeroes(buffer.begin(), page_size);
    bool dblwr_zeroes = buf_page_is_zeroes(page, page_size);
    dberr_t dblwr_err;
    const bool dblwr_corrupted =
        is_dblwr_page_corrupted(page, space, page_no, &dblwr_err);

    if (data_page_zeroes && !dblwr_zeroes && !dblwr_corrupted) {
      /* Database page contained only zeroes, while a valid copy is
      available in dblwr buffer. */
    } else {
      /* Database page is fine.  No need to restore from dblwr. */
      return false;
    }
  }

  ut_ad(!Encryption::is_encrypted_page(page));

  bool found = false;
  lsn_t reduced_lsn = LSN_MAX;
  std::tie(found, reduced_lsn) = find_entry(page_id);
  lsn_t dblwr_lsn = mach_read_from_8(page + FIL_PAGE_LSN);

  /* If we find a newer version of page that is in reduced dblwr, we
  shouldn't restore the old/stale page from regular dblwr. We should
  abort */
  if (found && reduced_lsn != LSN_MAX && reduced_lsn > dblwr_lsn) {
    ib::fatal(UT_LOCATION_HERE, ER_REDUCED_DBLWR_PAGE_FOUND,
              space->files.front().name, page_id.space(), page_id.page_no());
  }

  /* Recovered data file pages are written out as uncompressed. */
  IORequest write_request(IORequest::WRITE);
  write_request.disable_compression();

  /* Write the good page from the doublewrite buffer to the
  intended position. */

  err = fil_io(write_request, true, page_id, page_size, 0, page_size.physical(),
               const_cast<byte *>(page), nullptr);

  ut_a(err == DB_SUCCESS || err == DB_TABLESPACE_DELETED);

  ib::info(ER_IB_MSG_DBLWR_1308)
      << "Recovered page " << page_id << " from the doublewrite buffer.";

  return true;
}

void dblwr::force_flush(buf_flush_t flush_type,
                        uint32_t buf_pool_index) noexcept {
  Double_write::force_flush(flush_type, buf_pool_index);
}

void dblwr::force_flush_all() noexcept {
  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    force_flush(BUF_FLUSH_LRU, i);
    force_flush(BUF_FLUSH_LIST, i);
  }
}

#endif /* !UNIV_HOTBACKUP */

void recv::Pages::recover(fil_space_t *space) noexcept {
#ifndef UNIV_HOTBACKUP
  /* For cloned database double write pages should be ignored. However,
  given the control flow, we read the pages in anyway but don't recover
  from the pages we read in. */

  if (!dblwr::is_enabled() || recv_sys->is_cloned_db) {
    return;
  }

  auto recover_all = (space == nullptr);

  for (const auto &page : m_pages) {
    if (page->m_recovered) {
      continue;
    }

    auto ptr = page->m_buffer.begin();
    auto page_no = page_get_page_no(ptr);
    auto space_id = page_get_space_id(ptr);

    if (recover_all) {
      space = fil_space_get(space_id);

      if (space == nullptr) {
        /* Maybe we have dropped the tablespace
        and this page once belonged to it: do nothing. */
        continue;
      }

    } else if (space->id != space_id) {
      continue;
    }

    fil_space_open_if_needed(space);

    page->m_recovered =
        dblwr_recover_page(page->m_no, space, page_no, page->m_buffer.begin());
  }

  reduced_recover(space);
  fil_flush_file_spaces();
#endif /* !UNIV_HOTBACKUP */
}

void recv::Pages::reduced_recover(fil_space_t *space) noexcept {
#ifndef UNIV_HOTBACKUP
  auto recover_all = (space == nullptr);

  for (const auto &entry : m_page_entries) {
    auto space_id = entry.m_space_id;
    page_id_t page_id(entry.m_space_id, entry.m_page_no);

    if (recover_all) {
      space = fil_space_get(space_id);

      if (space == nullptr) {
        /* Maybe we have dropped the tablespace
        and this page once belonged to it: do nothing. */
        continue;
      }

    } else if (space->id != space_id) {
      continue;
    }

    fil_space_open_if_needed(space);

    bool is_corrupted = false;
    bool is_all_zero = false;
    std::tie(is_corrupted, is_all_zero) =
        is_actual_page_corrupted(space, page_id);

    if (is_corrupted) {
      if (find(page_id) == nullptr || !is_recovered(page_id)) {
        ib::fatal(UT_LOCATION_HERE, ER_REDUCED_DBLWR_PAGE_FOUND,
                  space->files.front().name, page_id.space(),
                  page_id.page_no());
      }
    }

    if (is_all_zero) {
      // is there a dblwr reduced entry with non-zero LSN?
      bool found = false;
      lsn_t reduced_lsn = LSN_MAX;
      std::tie(found, reduced_lsn) = find_entry(page_id);

      if (!is_recovered(page_id) && found && reduced_lsn != LSN_MAX &&
          reduced_lsn != 0) {
        ib::fatal(UT_LOCATION_HERE, ER_REDUCED_DBLWR_PAGE_FOUND,
                  space->files.front().name, page_id.space(),
                  page_id.page_no());
      }
    }
  }
#endif /* !UNIV_HOTBACKUP */
}

const byte *recv::Pages::find(const page_id_t &page_id) const noexcept {
  if (!dblwr::is_enabled()) {
    return nullptr;
  }
  using Matches = std::vector<const byte *, ut::allocator<const byte *>>;

  Matches matches;
  const byte *page = nullptr;

  for (const auto &page : m_pages) {
    auto &buffer = page->m_buffer;

    if (page_get_space_id(buffer.begin()) == page_id.space() &&
        page_get_page_no(buffer.begin()) == page_id.page_no()) {
      matches.push_back(buffer.begin());
    }
  }

  if (matches.size() == 1) {
    page = matches[0];

  } else if (matches.size() > 1) {
    lsn_t max_lsn = 0;

    for (const auto &match : matches) {
      lsn_t page_lsn = mach_read_from_8(match + FIL_PAGE_LSN);

      if (page_lsn > max_lsn) {
        max_lsn = page_lsn;
        page = match;
      }
    }
  }

  return page;
}

bool recv::Pages::is_recovered(const page_id_t &page_id) const noexcept {
  for (const auto &page : m_pages) {
    auto &buffer = page->m_buffer;

    if (page_get_space_id(buffer.begin()) == page_id.space() &&
        page_get_page_no(buffer.begin()) == page_id.page_no() &&
        page->m_recovered) {
      return (true);
    }
  }
  return (false);
}

void recv::Pages::add(page_no_t page_no, const byte *page,
                      uint32_t n_bytes) noexcept {
  if (!dblwr::is_enabled()) {
    return;
  }
  /* Make a copy of the page contents. */
  auto dblwr_page =
      ut::new_withkey<Page>(UT_NEW_THIS_FILE_PSI_KEY, page_no, page, n_bytes);

  m_pages.push_back(dblwr_page);
}

void recv::Pages::check_missing_tablespaces() const noexcept {
  /* For cloned database double write pages should be ignored. However,
  given the control flow, we read the pages in anyway but don't recover
  from the pages we read in. */
  if (!dblwr::is_enabled()) {
    return;
  }

  const auto end = recv_sys->deleted.end();

  for (const auto &page : m_pages) {
    if (page->m_recovered) {
      continue;
    }

    const auto &buffer = page->m_buffer;
    auto space_id = page_get_space_id(buffer.begin());

    /* Skip messages for undo tablespaces that are being truncated since
    they can be deleted during undo truncation without an MLOG_FILE_DELETE.
  */

    if (!fsp_is_undo_tablespace(space_id)) {
      /* If the tablespace was in the missing IDs then we
      know that the problem is elsewhere. If a file deleted
      record was not found in the redo log and the tablespace
      doesn't exist in the SYS_TABLESPACES file then it is
      an error or data corruption. The special case is an
      undo truncate in progress. */

      if (recv_sys->deleted.find(space_id) == end &&
          recv_sys->missing_ids.find(space_id) != recv_sys->missing_ids.end()) {
        auto page_no = page_get_page_no(buffer.begin());

        ib::warn(ER_IB_MSG_DBLWR_1296)
            << "Doublewrite page " << page->m_no << " for {space: " << space_id
            << ", page_no:" << page_no << "} could not be restored."
            << " File name unknown for tablespace ID " << space_id;
      }
    }
  }
}

dberr_t dblwr::recv::load(recv::Pages *pages) noexcept {
#ifndef UNIV_HOTBACKUP
  /* For cloned database double write pages should be ignored. */
  if (!dblwr::is_enabled()) {
    return DB_SUCCESS;
  }

  ut_ad(!dblwr::dir.empty());

  /* The number of buffer pool instances can change. Therefore we must:
    1. Scan the doublewrite directory for all *.dblwr files and load
       their contents.
    2. Reset the file sizes after recovery is complete. */

  auto real_path_dir = Fil_path::get_real_path(dblwr::dir);

  /* Walk the sub-tree of dblwr::dir. */

  std::vector<std::string> dblwr_files;

  Dir_Walker::walk(real_path_dir, false, [&](const std::string &path) {
    ut_a(path.length() > real_path_dir.length());

    if (Fil_path::get_file_type(path) != OS_FILE_TYPE_FILE) {
      return;
    }

    /* Make the filename relative to the directory that was scanned. */

    auto file = path.substr(real_path_dir.length(), path.length());

    /** 6 == strlen(".dblwr"). */
    if (file.size() <= 6) {
      return;
    }

    if (Fil_path::has_suffix(DWR, file.c_str())) {
      dblwr_files.push_back(file);
    }
  });

  /* We have to use all the dblwr files for recovery. */

  std::string rexp{"#ib_([0-9]+)_([0-9]+)\\"};

  rexp.append(dot_ext[DWR]);

  const std::regex regex{rexp};

  std::vector<int> ids;

  for (auto &file : dblwr_files) {
    std::smatch match;

    if (std::regex_match(file, match, regex) && match.size() == 3) {
      /* Check if the page size matches. */
      int page_size = std::stoi(match[1].str());

      if (page_size == (int)srv_page_size) {
        int id = std::stoi(match[2].str());
        ids.push_back(id);
      } else {
        ib::info(ER_IB_MSG_DBLWR_1310)
            << "Ignoring " << file << " - page size doesn't match";
      }
    } else {
      ib::warn(ER_IB_MSG_DBLWR_1311)
          << file << " not in double write buffer file name format!";
    }
  }

  std::sort(ids.begin(), ids.end());

  for (uint32_t i = 0; i < ids.size(); ++i) {
    if ((uint32_t)ids[i] != i) {
      ib::warn(ER_IB_MSG_DBLWR_1312) << "Gap in the double write buffer files.";
      ut_d(ut_error);
    }
  }

  uint32_t max_id;

  if (!ids.empty()) {
    max_id = std::max((int)srv_buf_pool_instances, ids.back() + 1);
  } else {
    max_id = srv_buf_pool_instances;
  }

  for (uint32_t i = 0; i < max_id; ++i) {
    dblwr::File file;

    /* Open the file for reading. */
    auto err = dblwr_file_open(dblwr::dir, i, file, OS_DATA_FILE);

    if (err == DB_NOT_FOUND) {
      continue;
    } else if (err != DB_SUCCESS) {
      return err;
    }

    err = Double_write::load(file, pages);

    os_file_close(file.m_pfs);

    if (err != DB_SUCCESS) {
      return err;
    }
  }
#endif /* UNIV_HOTBACKUP */
  return DB_SUCCESS;
}

dberr_t dblwr::recv::reduced_load(recv::Pages *pages) noexcept {
#ifndef UNIV_HOTBACKUP
  /* For cloned database double write pages should be ignored. */
  if (!dblwr::is_enabled()) {
    return DB_SUCCESS;
  }

  ut_ad(!dblwr::dir.empty());

  /* The number of buffer pool instances can change. Therefore we must:
  1. Scan the doublewrite directory for all *.dblwr files and load
     their contents.
  2. Reset the file sizes after recovery is complete. */

  auto real_path_dir = Fil_path::get_real_path(dblwr::dir);

  /* Walk the sub-tree of dblwr::dir. */

  std::vector<std::string> dblwr_files;

  Dir_Walker::walk(real_path_dir, false, [&](const std::string &path) {
    ut_a(path.length() > real_path_dir.length());

    if (Fil_path::get_file_type(path) != OS_FILE_TYPE_FILE) {
      return;
    }

    /* Make the filename relative to the directory that was scanned. */

    auto file = path.substr(real_path_dir.length(), path.length());

    if (file.size() <= strlen(dot_ext[BWR])) {
      return;
    }

    if (Fil_path::has_suffix(BWR, file.c_str())) {
      dblwr_files.push_back(file);
    }
  });
  /* We have to use all the dblwr files for recovery. */

  std::string rexp{"#ib_([0-9]+)_([0-9]+)\\"};

  rexp.append(dot_ext[BWR]);

  const std::regex regex{rexp};

  std::vector<int> ids;

  for (auto &file : dblwr_files) {
    std::smatch match;

    if (std::regex_match(file, match, regex) && match.size() == 3) {
      /* Check if the page size matches. */
      int page_size = std::stoi(match[1].str());

      if (page_size == (int)srv_page_size) {
        int id = std::stoi(match[2].str());
        ids.push_back(id);
      } else {
        ib::info(ER_IB_MSG_DBLWR_1310)
            << "Ignoring " << file << " - page size doesn't match";
      }
    } else {
      ib::warn(ER_IB_MSG_DBLWR_1311)
          << file << " not in double write buffer file name format!";
    }
  }

  if (ids.size() == 0) {
    // We are starting on older version that doesn't have reduced dblwr file
    return (DB_SUCCESS);
  }

  // There should be always only one Batch DBLWR file
  ut_ad(ids.size() == 1);

  dblwr::File file;

  /* Open the file for reading. */
  auto err = dblwr_file_open(dblwr::dir, 0, file, OS_DATA_FILE, BWR);

  if (err == DB_NOT_FOUND) {
    return (DB_SUCCESS);
  } else if (err != DB_SUCCESS) {
    return err;
  }

  err = Double_write::load_reduced_batch(file, pages);

  os_file_close(file.m_pfs);

  if (err != DB_SUCCESS) {
    return err;
  }

#endif /* UNIV_HOTBACKUP */
  return DB_SUCCESS;
}

const byte *dblwr::recv::find(const recv::Pages *pages,
                              const page_id_t &page_id) noexcept {
  return pages->find(page_id);
}

std::tuple<bool, lsn_t> dblwr::recv::find_entry(
    const recv::Pages *pages, const page_id_t &page_id) noexcept {
  return pages->find_entry(page_id);
}

void dblwr::recv::create(recv::Pages *&pages) noexcept {
  ut_a(pages == nullptr);
  pages = ut::new_withkey<recv::Pages>(UT_NEW_THIS_FILE_PSI_KEY);
}

void dblwr::recv::destroy(recv::Pages *&pages) noexcept {
  if (pages != nullptr) {
    ut::delete_(pages);
    pages = nullptr;
  }
}

void dblwr::recv::check_missing_tablespaces(const recv::Pages *pages) noexcept {
  pages->check_missing_tablespaces();
}

namespace dblwr {

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
static bool is_encrypted_page(const byte *page) noexcept {
  ulint page_type = mach_read_from_2(page + FIL_PAGE_TYPE);

  return page_type == FIL_PAGE_ENCRYPTED ||
         page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED ||
         page_type == FIL_PAGE_ENCRYPTED_RTREE;
}

bool has_encrypted_pages() noexcept {
  bool st = false;
  for (dblwr::File &file : Double_write::s_files) {
    dblwr::recv::Pages pages;
    TLOG("Loading= " << file);

    dberr_t err = Double_write::load(file, &pages);
    if (err != DB_SUCCESS) {
      TLOG("Failed to load= " << file);
      return st;
    }

    for (const auto &page : pages.get_pages()) {
      auto &buffer = page->m_buffer;
      byte *frame = buffer.begin();
      page_type_t page_type = fil_page_get_type(frame);

      TLOG("space_id=" << page_get_space_id(frame)
                       << ", page_no=" << page_get_page_no(frame)
                       << ", page_type=" << fil_get_page_type_str(page_type));

      if (is_encrypted_page(frame)) {
        st = true;
      }
    }
  }
  return st;
}
#endif /* UNIV_DEBUG */

Reduced_entry::Reduced_entry(buf_page_t *bpage)
    : m_space_id(bpage->space()),
      m_page_no(bpage->page_no()),
      m_lsn(bpage->get_newest_lsn()) {}

#endif /* !UNIV_HOTBACKUP */

}  // namespace dblwr
