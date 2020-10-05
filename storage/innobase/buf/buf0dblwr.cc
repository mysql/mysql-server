/*****************************************************************************

Copyright (c) 1995, 2020, Oracle and/or its affiliates.

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

bool enabled{true};

/** Legacy dblwr buffer first segment page number. */
static page_no_t LEGACY_PAGE1;

/** Legacy dblwr buffer second segment page number. */
static page_no_t LEGACY_PAGE2;

struct File {
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
  std::string to_json() const noexcept MY_ATTRIBUTE((warn_unused_result)) {
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
  @param[in]	page_no	          Page number in the doublewrite buffer
  @param[in]	page	            Page read from the double write buffer
  @param[in]	n_bytes	          Length of the page data. */
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

/** Pages recovered from the doublewrite buffer */
class Pages {
 public:
  using Buffers = std::vector<Page *, ut_allocator<Page *>>;

  /** Default constructor */
  Pages() : m_pages() {}

  /** Destructor */
  ~Pages() noexcept {
    for (auto &page : m_pages) {
      UT_DELETE(page);
    }

    m_pages.clear();
  }

  /** Add a page frame to the doublewrite recovery buffer.
  @param[in]	page_no		        Page number in the doublewrite buffer
  @param[in]	page		          Page contents
  @param[in]	n_bytes		        Size in bytes */
  void add(page_no_t page_no, const byte *page, uint32_t n_bytes) noexcept;

  /** Find a doublewrite copy of a page.
  @param[in]	page_id		        Page number to lookup
  @return	page frame
  @retval nullptr if no page was found */
  const byte *find(const page_id_t &page_id) const noexcept;

  /** Recover double write buffer pages
  @param[in]	space		          Tablespace pages to recover, if set
                                to nullptr then try and recovery all. */
  void recover(fil_space_t *space) noexcept;

  /** Check if some pages could be restored because of missing
  tablespace IDs */
  void check_missing_tablespaces() const noexcept;

  /** Object the vector of pages.
  @return the vector of pages. */
  Buffers &get_pages() noexcept MY_ATTRIBUTE((warn_unused_result)) {
    return m_pages;
  }

 private:
  /** Recovered doublewrite buffer page frames */
  Buffers m_pages;

  // Disable copying
  Pages(const Pages &) = delete;
  Pages(const Pages &&) = delete;
  Pages &operator=(Pages &&) = delete;
  Pages &operator=(const Pages &) = delete;
};

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
  /** Maximum wait in micro-seconds for new write events. */
  static constexpr auto MAX_WAIT_FOR_EVENTS = 10000000;

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
    @param[in] e_block   encrypted block.
    @param[in] e_len     length of data in e_block. */
    void push_back(buf_page_t *bpage, const file::Block *e_block,
                   uint32_t e_len) noexcept {
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
      m_pages[m_size++] = std::make_tuple(bpage, e_block, e_len);
    }

    /** Clear the collection. */
    void clear() noexcept { m_size = 0; }

    /** @return check if collection is empty. */
    bool empty() const noexcept { return size() == 0; }

    /** @return number of active elements. */
    uint32_t size() const noexcept { return m_size; }

    /** @return the capacity of the collection. */
    uint32_t capacity() const noexcept MY_ATTRIBUTE((warn_unused_result)) {
      return m_pages.capacity();
    }

    typedef std::tuple<buf_page_t *, const file::Block *, uint32_t> Dblwr_tuple;
    using Pages = std::vector<Dblwr_tuple, ut_allocator<Dblwr_tuple>>;

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
  ~Double_write() noexcept;

  /** @return instance ID */
  uint16_t id() const noexcept MY_ATTRIBUTE((warn_unused_result)) {
    return m_id;
  }

  /** Process the requests in the flush queue, write the blocks to the
  double write file, sync the file if required and then write to the
  data files. */
  void write(buf_flush_t flush_type) noexcept;

  /** @return the double write instance to use for flushing.
  @param[in] buf_pool_index     Buffer pool instance number.
  @param[in] flush_type         LRU or Flush list write.
  @return instance that will handle the flush to disk. */
  static Double_write *instance(buf_flush_t flush_type,
                                uint32_t buf_pool_index) noexcept
      MY_ATTRIBUTE((warn_unused_result)) {
    ut_a(buf_pool_index < srv_buf_pool_instances);

    auto midpoint = s_instances->size() / 2;
    auto i = midpoint > 0 ? buf_pool_index % midpoint : 0;

    if (flush_type == BUF_FLUSH_LIST) {
      i += midpoint;
    }

    return s_instances->at(i);
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
  @param[in] e_block        Encrypted block frame or nullptr.
  @param[in] e_len          Encrypted data length if e_block is valid. */
  void enqueue(buf_flush_t flush_type, buf_page_t *bpage,
               const file::Block *e_block, uint32_t e_len) noexcept {
    ut_ad(buf_page_in_file(bpage));

    void *frame{};
    uint32_t len{};
    byte *e_frame =
        (e_block == nullptr) ? nullptr : os_block_get_frame(e_block);

    if (e_frame != nullptr) {
      frame = e_frame;
      len = e_len;
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

    m_buf_pages.push_back(bpage, e_block, e_len);

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
  static dberr_t create_batch_segments(uint32_t segments_per_file) noexcept
      MY_ATTRIBUTE((warn_unused_result));

  /** Create the single page flush segments.
  @param[in] segments_per_file  Number of configured segments per file.
  @return DB_SUCCESS or error code. */
  static dberr_t create_single_segments(uint32_t segments_per_file) noexcept
      MY_ATTRIBUTE((warn_unused_result));

  /** Get the instance that handles a particular page's IO. Submit the
  write request to the a double write queue that is empty.
  @param[in]  flush_type        Flush type.
  @param[in]	bpage             Page from the buffer pool.
  @param[in]  e_block    compressed + encrypted frame contents or nullptr.
  @param[in]  e_len      encrypted data length. */
  static void submit(buf_flush_t flush_type, buf_page_t *bpage,
                     const file::Block *e_block, uint32_t e_len) noexcept {
    if (s_instances == nullptr) {
      return;
    }

    auto dblwr = instance(flush_type, bpage);
    dblwr->enqueue(flush_type, bpage, e_block, e_len);
  }

  /** Writes a single page to the doublewrite buffer on disk, syncs it,
  then writes the page to the datafile.
  @param[in]	bpage             Data page to write to disk.
  @param[in]	e_block           Encrypted data block.
  @param[in]	e_len             Encrypted data length.
  @return DB_SUCCESS or error code */
  static dberr_t sync_page_flush(buf_page_t *bpage, file::Block *e_block,
                                 uint32_t e_len) noexcept
      MY_ATTRIBUTE((warn_unused_result));

  // clang-format off
  /** @return the double write instance to use for flushing.
  @param[in] flush_type         LRU or Flush list write.
  @param[in] bpage              Page to write to disk.
  @return instance that will handle the flush to disk. */
  static Double_write *instance(buf_flush_t flush_type, const buf_page_t *bpage)
      noexcept MY_ATTRIBUTE((warn_unused_result)) {
    return instance(flush_type, buf_pool_index(buf_pool_from_bpage(bpage)));
  }

  /** Updates the double write buffer when a write request is completed.
  @param[in,out] bpage          Block that has just been written to disk.
  @param[in] flush_type         Flush type that triggered the write. */
  static void write_complete(buf_page_t *bpage, buf_flush_t flush_type)
      noexcept;

  /** REad the V1 doublewrite buffer extents boundaries.
  @param[in,out] block1         Starting block number for the first extent.
  @param[in,out] block2         Starting block number for the second extent.
  @return true if successful, false if not. */
  static bool init_v1(page_no_t &block1, page_no_t &block2) noexcept
      MY_ATTRIBUTE((warn_unused_result));

  /** Creates the V1 doublewrite buffer extents. The header of the
  doublewrite buffer is placed on the trx system header page.
  @param[in,out] block1         Starting block number for the first extent.
  @param[in,out] block2         Starting block number for the second extent.
  @return true if successful, false if not. */
  static bool create_v1(page_no_t &block1, page_no_t &block2) noexcept
      MY_ATTRIBUTE((warn_unused_result));

  /** Writes a page that has already been written to the
  doublewrite buffer to the data file. It is the job of the
  caller to sync the datafile.
  @param[in]  in_bpage          Page to write.
  @param[in]  sync              true if it's a synchronous write.
  @param[in]  e_block           block containing encrypted data frame.
  @param[in]  e_len             encrypted data length.
  @return DB_SUCCESS or error code */
  static dberr_t write_to_datafile(const buf_page_t *in_bpage, bool sync,
      const file::Block* e_block, uint32_t e_len)
      noexcept MY_ATTRIBUTE((warn_unused_result));

  /** Force a flush of the page queue.
  @param[in] flush_type           FLUSH LIST or LRU LIST flush.
  @param[in] buf_pool_index       Buffer pool instance for which called. */
  static void force_flush(buf_flush_t flush_type, uint32_t buf_pool_index)
      noexcept {
    if (s_instances == nullptr) {
      return;
    }
    auto dblwr = instance(flush_type, buf_pool_index);

    dblwr->force_flush(flush_type);
  }

  /** Load the doublewrite buffer pages from an external file.
  @param[in,out]	file		      File handle
  @param[in,out]	pages		      For storing the doublewrite pages
                                read from the file
  @return DB_SUCCESS or error code */
  static dberr_t load(dblwr::File &file, recv::Pages *pages) noexcept
      MY_ATTRIBUTE((warn_unused_result));

  /** Write zeros to the file if it is "empty"
  @param[in]	file		          File instance.
  @param[in]	n_pages           Size in physical pages.
  @return DB_SUCCESS or error code */
  static dberr_t init_file(dblwr::File &file, uint32_t n_pages) noexcept
      MY_ATTRIBUTE((warn_unused_result));

  /** Reset the size in bytes to the configured size.
  @param[in,out] file						File to reset.
  @param[in] truncate           Truncate the file to configured size if true. */
  static void reset_file(dblwr::File &file, bool truncate) noexcept;

  /** Reset the size in bytes to the configured size of all files. */
  static void reset_files() noexcept {
    for (auto &file : Double_write::s_files) {
      /* Physically truncate the file: true. */
      Double_write::reset_file(file, true);
    }
  }

  /** Create the v2 data structures
  @return DB_SUCCESS or error code */
  static dberr_t create_v2() noexcept MY_ATTRIBUTE((warn_unused_result));

#ifndef _WIN32
  /** @return true if we need to fsync to disk */
  static bool is_fsync_required() noexcept MY_ATTRIBUTE((warn_unused_result)) {
    /* srv_unix_file_flush_method is a dynamic variable. */
    return srv_unix_file_flush_method != SRV_UNIX_O_DIRECT &&
           srv_unix_file_flush_method != SRV_UNIX_O_DIRECT_NO_FSYNC;
  }
#endif /* _WIN32 */

  /** Extract the data and length to write to the doublewrite file
  @param[in]	bpage		          Page to write
  @param[out]	ptr		            Start of buffer to write
  @param[out]	len		            Length of the data to write */
  static void prepare(const buf_page_t *bpage, void **ptr, uint32_t *len)
      noexcept;

  /** Free the data structures. */
  static void shutdown() noexcept;

  /** Toggle the doublewrite buffer dynamically
  @param[in]	value		          Current value */
  static void toggle(bool value) noexcept {
    if (s_instances == nullptr) {
      return;
    }

    if (value) {
      ib::info(ER_IB_MSG_DBLWR_1304) << "Atomic write enabled";
    } else {
      ib::info(ER_IB_MSG_DBLWR_1305) << "Atomic write disabled";
    }
  }

  // clang-format on

  /** Write the data to disk synchronously.
  @param[in]    segment      Segment to write to.
  @param[in]	bpage        Page to write.
  @param[in]    e_block      Encrypted block.  Can be nullptr.
  @param[in]    e_len        Encrypted data length in e_block. */
  static void single_write(Segment *segment, const buf_page_t *bpage,
                           file::Block *e_block, uint32_t e_len) noexcept;

 private:
  /** Create the singleton instance, start the flush thread
  @return DB_SUCCESS or error code */
  static dberr_t start() noexcept MY_ATTRIBUTE((warn_unused_result));

  /** Asserts when a corrupt block is found during writing out
  data to the disk.
  @param[in]	block		          Block that was corrupt */
  static void croak(const buf_block_t *block) noexcept;

  /** Check the LSN values on the page with which this block
  is associated.  Also validate the page if the option is set.
  @param[in]	block		          Block to check */
  static void check_block(const buf_block_t *block) noexcept;

  /** Check the LSN values on the page.
  @param[in]	page		          Page to check */
  static void check_page_lsn(const page_t *page) noexcept;

  /** Calls buf_page_get() on the TRX_SYS_PAGE and returns
  a pointer to the doublewrite buffer within it.
  @param[in,out]	mtr		        To manage the page latches
  @return pointer to the doublewrite buffer within the filespace
          header page. */
  static byte *get(mtr_t *mtr) noexcept MY_ATTRIBUTE((warn_unused_result));

 private:
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

  /** For indexing batch segments by ID. */
  static std::vector<Batch_segment *> s_segments;

 public:
  /** Files to use for atomic writes. */
  static std::vector<dblwr::File> s_files;

  /** The global instances */
  static Instances *s_instances;

  // Disable copying
  Double_write(const Double_write &) = delete;
  Double_write(const Double_write &&) = delete;
  Double_write &operator=(Double_write &&) = delete;
  Double_write &operator=(const Double_write &) = delete;
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
        m_start(start * univ_page_size.physical()),
        m_end(m_start + (n_pages * univ_page_size.physical())) {}

  /** Destructor. */
  virtual ~Segment() {}

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

  /** Destructor. */
  ~Batch_segment() noexcept override {
    ut_a(m_written.load(std::memory_order_relaxed) == 0);
    ut_a(m_batch_size.load(std::memory_order_relaxed) == 0);
  }

  /** @return the batch segment ID. */
  uint16_t id() const noexcept { return m_id; }

  /**  Write a batch to the segment.
  @param[in] buffer             Buffer to write. */
  void write(const Buffer &buffer) noexcept;

  /** Called on page write completion.
  @return if batch ended. */
  bool write_complete() noexcept MY_ATTRIBUTE((warn_unused_result)) {
    const auto n = m_written.fetch_add(1, std::memory_order_relaxed);
    return n + 1 == m_batch_size.load(std::memory_order_relaxed);
  }

  /** Reset the state. */
  void reset() noexcept {
    m_written.store(0, std::memory_order_relaxed);
    m_batch_size.store(0, std::memory_order_relaxed);
  }

  /** Set the batch size.
  @param[in] size               Number of pages to write to disk. */
  void set_batch_size(uint32_t size) noexcept {
    m_batch_size.store(size, std::memory_order_release);
  }

  /** @return the batch size. */
  uint32_t batch_size() const noexcept {
    return m_batch_size.load(std::memory_order_acquire);
  }

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

  /** Size of the batch. */
  std::atomic_int m_batch_size{};

  byte m_pad2[ut::INNODB_CACHE_LINE_SIZE];

  /** Number of pages to write. */
  std::atomic_int m_written{};
};

uint32_t Double_write::s_n_instances{};
std::vector<dblwr::File> Double_write::s_files;
Double_write::Segments *Double_write::s_single_segments{};
Double_write::Batch_segments *Double_write::s_LRU_batch_segments{};
Double_write::Batch_segments *Double_write::s_flush_list_batch_segments{};
std::vector<Batch_segment *> Double_write::s_segments{};

Double_write::Instances *Double_write::s_instances{};

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
      ib::fatal(ER_IB_MSG_DBLWR_1297)
          << "Invalid page state: state: " << state
          << " block state: " << buf_page_get_state(bpage);
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
                                file::Block *e_block, uint32_t e_len) noexcept {
  uint32_t len{};
  void *frame{};

  if (e_block != nullptr) {
    frame = os_block_get_frame(e_block);
    len = e_len;
  } else {
    prepare(bpage, &frame, &len);
  }

  ut_ad(len <= univ_page_size.physical());

  segment->write(frame, len);
}

void Batch_segment::write(const Buffer &buffer) noexcept {
  Segment::write(buffer.begin(), buffer.size());
}

dberr_t Double_write::create_v2() noexcept {
  ut_a(!s_files.empty());
  ut_a(s_instances == nullptr);

  s_instances = UT_NEW_NOKEY(Instances{});

  if (s_instances == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  dberr_t err{DB_SUCCESS};

  for (uint32_t i = 0; i < s_n_instances; ++i) {
    auto ptr = UT_NEW_NOKEY(Double_write(i, dblwr::n_pages));

    if (ptr == nullptr) {
      err = DB_OUT_OF_MEMORY;
      break;
    }

    s_instances->push_back(ptr);
  }

  if (err != DB_SUCCESS) {
    for (auto &dblwr : *s_instances) {
      UT_DELETE(dblwr);
    }
    UT_DELETE(s_instances);
    s_instances = nullptr;
  }

  return err;
}

void Double_write::shutdown() noexcept {
  if (s_instances == nullptr) {
    return;
  }

  for (auto dblwr : *s_instances) {
    UT_DELETE(dblwr);
  }

  for (auto &file : s_files) {
    if (file.m_pfs.m_file != OS_FILE_CLOSED) {
      os_file_close(file.m_pfs);
    }
  }

  s_files.clear();

  if (s_LRU_batch_segments != nullptr) {
    Batch_segment *s{};
    while (s_LRU_batch_segments->dequeue(s)) {
      UT_DELETE(s);
    }
    UT_DELETE(s_LRU_batch_segments);
    s_LRU_batch_segments = nullptr;
  }

  if (s_flush_list_batch_segments != nullptr) {
    Batch_segment *s{};
    while (s_flush_list_batch_segments->dequeue(s)) {
      UT_DELETE(s);
    }
    UT_DELETE(s_flush_list_batch_segments);
    s_flush_list_batch_segments = nullptr;
  }

  if (s_single_segments != nullptr) {
    Segment *s{};
    while (s_single_segments->dequeue(s)) {
      UT_DELETE(s);
    }
    UT_DELETE(s_single_segments);
    s_single_segments = nullptr;
  }

  UT_DELETE(s_instances);
  s_instances = nullptr;
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

  ib::fatal(ER_IB_MSG_112)
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
                                        const file::Block *e_block,
                                        uint32_t e_len) noexcept {
  ut_ad(buf_page_in_file(in_bpage));
  uint32_t len;
  void *frame{};

  if (e_block == nullptr) {
    Double_write::prepare(in_bpage, &frame, &len);
  } else {
    frame = os_block_get_frame(e_block);
    len = e_len;
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

  auto err =
      fil_io(io_request, sync, bpage->id, bpage->size, 0, len, frame, bpage);

  /* When a tablespace is deleted with BUF_REMOVE_NONE, fil_io() might
  return DB_PAGE_IS_STALE or DB_TABLESPACE_DELETED. */
  ut_a(err == DB_SUCCESS || err == DB_TABLESPACE_DELETED ||
       err == DB_PAGE_IS_STALE);

  return err;
}

dberr_t Double_write::sync_page_flush(buf_page_t *bpage, file::Block *e_block,
                                      uint32_t e_len) noexcept {
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
    os_thread_yield();
  }

  single_write(segment, bpage, e_block, e_len);

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

  auto err = write_to_datafile(bpage, true, e_block, e_len);

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
  } else if ((file.m_id & 1)) {
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
      ib::fatal(ER_IB_MSG_DBLWR_1320, file.m_name.c_str());
    }

  } else if (new_size > cur_size) {
    auto err = os_file_write_zeros(pfs_file, file.m_name.c_str(),
                                   univ_page_size.physical(), cur_size,
                                   new_size - cur_size, srv_read_only_mode);

    if (err != DB_SUCCESS) {
      ib::fatal(ER_IB_MSG_DBLWR_1321, file.m_name.c_str());
    }

    ib::info(ER_IB_MSG_DBLWR_1307)
        << file.m_name << " size increased to " << new_size << " bytes "
        << "from " << cur_size << " bytes";
  }
}

dberr_t Double_write::init_file(dblwr::File &file, uint32_t n_pages) noexcept {
  auto pfs_file = file.m_pfs;
  auto size = os_file_get_size(pfs_file);

  ut_ad(dblwr::File::s_n_pages > 0);

  if (size == 0) {
    auto err = os_file_write_zeros(
        pfs_file, file.m_name.c_str(), univ_page_size.physical(), 0,
        n_pages * univ_page_size.physical(), srv_read_only_mode);

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

  auto block = buf_page_get(sys_page_id, univ_page_size, RW_X_LATCH, mtr);

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

void Double_write::write_pages(buf_flush_t flush_type) noexcept {
  ut_ad(mutex_own(&m_mutex));
  ut_a(!m_buffer.empty());

  Batch_segment *batch_segment{};

  auto segments = flush_type == BUF_FLUSH_LRU ? s_LRU_batch_segments
                                              : s_flush_list_batch_segments;

  while (!segments->dequeue(batch_segment)) {
    os_thread_yield();
  }

  batch_segment->start(this);

  batch_segment->write(m_buffer);

  m_buffer.clear();

#ifndef _WIN32
  if (is_fsync_required()) {
    batch_segment->flush();
  }
#endif /* !_WIN32 */

  batch_segment->set_batch_size(m_buf_pages.size());

  for (uint32_t i = 0; i < m_buf_pages.size(); ++i) {
    const auto bpage = std::get<0>(m_buf_pages.m_pages[i]);

    ut_d(auto page_id = bpage->id);

    bpage->set_dblwr_batch_id(batch_segment->id());

    auto err =
        write_to_datafile(bpage, false, std::get<1>(m_buf_pages.m_pages[i]),
                          std::get<2>(m_buf_pages.m_pages[i]));

    if (err == DB_PAGE_IS_STALE || err == DB_TABLESPACE_DELETED) {
      write_complete(bpage, flush_type);
      buf_page_free_stale_during_write(
          bpage, buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);

      const file::Block *block = std::get<1>(m_buf_pages.m_pages[i]);
      if (block != nullptr) {
        os_free_block(const_cast<file::Block *>(block));
      }
    } else {
      ut_a(err == DB_SUCCESS);
    }

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

dberr_t Double_write::create_batch_segments(
    uint32_t segments_per_file) noexcept {
  const uint32_t n_segments = segments_per_file * s_files.size();

  const auto n = std::max(ulint{2}, ut_2_power_up((n_segments + 1)));

  ut_a(s_LRU_batch_segments == nullptr);

  s_LRU_batch_segments = UT_NEW_NOKEY(Batch_segments(n));

  if (s_LRU_batch_segments == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  ut_a(s_flush_list_batch_segments == nullptr);

  s_flush_list_batch_segments = UT_NEW_NOKEY(Batch_segments(n));

  if (s_flush_list_batch_segments == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  const uint32_t total_pages = segments_per_file * dblwr::n_pages;

  uint16_t id{};

  for (auto &file : s_files) {
    for (uint32_t i = 0; i < total_pages; i += dblwr::n_pages, ++id) {
      auto s = UT_NEW_NOKEY(Batch_segment(id, file, i, dblwr::n_pages));

      if (s == nullptr) {
        return DB_OUT_OF_MEMORY;
      }

      Batch_segments *segments{};

      if (s_files.size() > 1) {
        segments = (file.m_id & 1) ? s_LRU_batch_segments
                                   : s_flush_list_batch_segments;
      } else {
        segments = id & 1 ? s_LRU_batch_segments : s_flush_list_batch_segments;
      }

      auto success = segments->enqueue(s);
      ut_a(success);
      s_segments.push_back(s);
    }
  }

  return DB_SUCCESS;
}

dberr_t Double_write::create_single_segments(
    uint32_t segments_per_file) noexcept {
  ut_a(s_single_segments == nullptr);

  const auto n_segments =
      std::max(ulint{2}, ut_2_power_up(SYNC_PAGE_FLUSH_SLOTS));

  s_single_segments = UT_NEW_NOKEY(Segments(n_segments));

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
    if (!(file.m_id & 1) && s_files.size() > 1) {
      /* Skip the flush list files. */
      continue;
    }
    const auto start = dblwr::File::s_n_pages;

    for (uint32_t i = start; i < start + n_pages; ++i) {
      auto s = UT_NEW_NOKEY(Segment(file, i, 1UL));

      if (s == nullptr) {
        return DB_OUT_OF_MEMORY;
      }

      auto success = s_single_segments->enqueue(s);
      ut_a(success);
    }
  }

  return DB_SUCCESS;
}

file::Block *dblwr::get_encrypted_frame(buf_page_t *bpage,
                                        uint32_t &e_len) noexcept {
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
  if (space->encryption_op_in_progress == DECRYPTION ||
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

  space->get_encryption_info(type.get_encryption_info());
  auto e_block = os_file_encrypt_page(type, frame, &n);

  if (compressed_block != nullptr) {
    file::Block::free(compressed_block);
  }

  e_len = n;
  return e_block;
}

dberr_t dblwr::write(buf_flush_t flush_type, buf_page_t *bpage,
                     bool sync) noexcept {
  dberr_t err;
  const space_id_t space_id = bpage->id.space();

  /* This is not required for correctness, but it aborts the processing early.
   */
  if (bpage->was_stale()) {
    /* Disable batch completion in write_complete(). */
    bpage->set_dblwr_batch_id(std::numeric_limits<uint16_t>::max());
    buf_page_free_stale_during_write(
        bpage, buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);
    return DB_SUCCESS;
  }

  if (srv_read_only_mode || fsp_is_system_temporary(space_id) ||
      !dblwr::enabled || Double_write::s_instances == nullptr ||
      mtr_t::s_logging.dblwr_disabled()) {
    /* Skip the double-write buffer since it is not needed. Temporary
    tablespaces are never recovered, therefore we don't care about
    torn writes. */
    bpage->set_dblwr_batch_id(std::numeric_limits<uint16_t>::max());
    err = Double_write::write_to_datafile(bpage, sync, nullptr, 0);
    if (err == DB_PAGE_IS_STALE || err == DB_TABLESPACE_DELETED) {
      buf_page_free_stale_during_write(
          bpage, buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);
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
    uint32_t e_len{};
    file::Block *e_block = dblwr::get_encrypted_frame(bpage, e_len);

    if (!sync && flush_type != BUF_FLUSH_SINGLE_PAGE) {
      MONITOR_INC(MONITOR_DBLWR_ASYNC_REQUESTS);

      Double_write::submit(flush_type, bpage, e_block, e_len);
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
      err = Double_write::sync_page_flush(bpage, e_block, e_len);
    }
  }

  return err;
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

          auto segments = (flush_type == BUF_FLUSH_LRU)
                              ? Double_write::s_LRU_batch_segments
                              : Double_write::s_flush_list_batch_segments;

          fil_flush_file_spaces(FIL_TYPE_TABLESPACE);

          while (!segments->enqueue(batch_segment)) {
            os_thread_yield();
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
@return DB_SUCCESS if all went well. */
static dberr_t dblwr_file_open(const std::string &dir_name, int id,
                               dblwr::File &file, ulint file_type) noexcept {
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

  file.m_name += dot_ext[DWR];

  uint32_t mode;
  if (dir_exists) {
    os_file_status(file.m_name.c_str(), &file_exists, &type);

    if (type == OS_FILE_TYPE_FILE) {
      mode = OS_FILE_OPEN;
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

dberr_t dblwr::open(bool create_new_db) noexcept {
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
    err = Double_write::create_single_segments(segments_per_file);
  }

  if (err == DB_SUCCESS) {
    err = Double_write::create_v2();
  } else {
    Double_write::shutdown();
  }

  return err;
}

void dblwr::close() noexcept { Double_write::shutdown(); }

void dblwr::set() {
#ifndef UNIV_HOTBACKUP
  Double_write::toggle(dblwr::enabled);
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
static bool is_dblwr_page_corrupted(const byte *page, fil_space_t *space,
                                    page_no_t page_no, dberr_t *err) noexcept {
  const page_size_t page_size(space->flags);
  const bool is_checksum_disabled = fsp_is_checksum_disabled(space->id);
  bool corrupted = false;

  BlockReporter dblwr_page(true, page, page_size, is_checksum_disabled);

  if (dblwr_page.is_encrypted()) {
    Encryption en;
    IORequest req_type;
    size_t z_page_size;

    space->get_encryption_info(en);
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

    *err = en.decrypt(req_type, const_cast<byte *>(page), z_page_size, nullptr,
                      z_page_size);
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
        *err =
            os_file_decompress_page(true, const_cast<byte *>(page), nullptr, 0);

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
@param[in]	dblwr_page_no	      Page number if the doublewrite buffer
@param[in]	space		            Tablespace the page belongs to
@param[in]	page_no		          Page number in the tablespace
@param[in]	page		            Data to write to <space, page_no>
@return true if page was restored to the tablespace */
static bool dblwr_recover_page(page_no_t dblwr_page_no, fil_space_t *space,
                               page_no_t page_no, const byte *page) noexcept {
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

      ib::fatal(ER_IB_MSG_DBLWR_1306);
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
#endif /* !UNIV_HOTBACKUP */

void recv::Pages::recover(fil_space_t *space) noexcept {
#ifndef UNIV_HOTBACKUP
  /* For cloned database double write pages should be ignored. However,
  given the control flow, we read the pages in anyway but don't recover
  from the pages we read in. */

  if (!dblwr::enabled || recv_sys->is_cloned_db) {
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

  fil_flush_file_spaces(FIL_TYPE_TABLESPACE);
#endif /* !UNIV_HOTBACKUP */
}

const byte *recv::Pages::find(const page_id_t &page_id) const noexcept {
  if (!dblwr::enabled) {
    return nullptr;
  }
  using Matches = std::vector<const byte *, ut_allocator<const byte *>>;

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

void recv::Pages::add(page_no_t page_no, const byte *page,
                      uint32_t n_bytes) noexcept {
  if (!dblwr::enabled) {
    return;
  }
  /* Make a copy of the page contents. */
  auto dblwr_page = UT_NEW_NOKEY(Page(page_no, page, n_bytes));

  m_pages.push_back(dblwr_page);
}

void recv::Pages::check_missing_tablespaces() const noexcept {
  /* For cloned database double write pages should be ignored. However,
  given the control flow, we read the pages in anyway but don't recover
  from the pages we read in. */
  if (!dblwr::enabled) {
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
  if (!dblwr::enabled) {
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
      ut_ad(0);
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

const byte *dblwr::recv::find(const recv::Pages *pages,
                              const page_id_t &page_id) noexcept {
  return pages->find(page_id);
}

void dblwr::recv::create(recv::Pages *&pages) noexcept {
  ut_a(pages == nullptr);
  pages = UT_NEW_NOKEY(recv::Pages{});
}

void dblwr::recv::destroy(recv::Pages *&pages) noexcept {
  if (pages != nullptr) {
    UT_DELETE(pages);
    pages = nullptr;
  }
}

void dblwr::recv::check_missing_tablespaces(const recv::Pages *pages) noexcept {
  pages->check_missing_tablespaces();
}

namespace dblwr {

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

}  // namespace dblwr
