/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
*/

#ifdef UNIV_LINUX
#include <sys/uio.h>
#endif /* UNIV_LINUX */

#include "fil0innodb_tablespace_node_handle.h"
#include "fil0innodb_tablespaces_nodes.h"
#include "fsp0fsp.h"
#include "srv0srv.h"
#include "trx0purge.h"

namespace ib::fil {

Tablespace_node_handle::Tablespace_node_handle(pfs_os_file_t handle,
                                               const std::string &file_name,
                                               space_id_t space_id,
                                               size_t page_size,
                                               size_t node_order)
    : m_handle(std::move(handle)),
      m_file_name(file_name),
      m_space_id(space_id),
      m_physical_page_size(page_size),
      m_node_order(node_order) {}

Tablespace_node_handle::~Tablespace_node_handle() {
  ut_a(m_handle.m_file != OS_FILE_CLOSED);
  const auto success = os_file_close(m_handle);
  ut_a(success);
}

bool Tablespace_node_handle::needs_flushes_for_durability() const {
  return true;
}

Tablespace_node_handle::Status Tablespace_node_handle::flush() {
  if (!os_file_flush(m_handle)) {
    return Status::IO_ERROR;
  }
  return Status::SUCCESS;
}

Tablespace_node_handle::Status Tablespace_node_handle::truncate(
    Page_number size) {
  ut_a(m_handle.m_file != OS_FILE_CLOSED);

  const auto size_after_truncate = size * m_physical_page_size;

  return os_file_truncate(m_file_name.c_str(), m_handle, size_after_truncate)
             ? Status::SUCCESS
             : Status::IO_ERROR;
}

Tablespace_node_handle::Status Tablespace_node_handle::fill_range_with_zeros(
    Page_number first_page, Page_number number_of_pages, bool optimize_writes) {
  /* We are not flushing the file, it is job of the `fil_flush()`. */
  const auto err = os_file_fill_range_with_zeros(
      m_file_name.c_str(), m_handle, first_page * m_physical_page_size,
      number_of_pages * m_physical_page_size, false, !optimize_writes);

  switch (err) {
    case DB_IO_NO_PUNCH_HOLE:
    case DB_SUCCESS:
      return Status::SUCCESS;
    case DB_IO_ERROR:
      return Status::IO_ERROR;
    default:
      ut_d(ut_error);
      ut_o(return Status::IO_ERROR);
  }
}

Tablespace_node_handle::Status_IO
Tablespace_node_handle::map_db_err_to_status_io(dberr_t err) {
  switch (err) {
    case DB_SUCCESS:
      return Status_IO::SUCCESS;
    case DB_IO_DECRYPT_FAIL:
      return Status_IO::IO_DECRYPT_FAIL;
    case DB_CORRUPTION:
      return Status_IO::CORRUPTION;
    case DB_OVERFLOW:
      return Status_IO::IO_OVERFLOW;
    case DB_OUT_OF_MEMORY:
      return Status_IO::OUT_OF_MEMORY;
    case DB_IO_DECOMPRESS_FAIL:
      return Status_IO::IO_DECOMPRESS_FAIL;
    case DB_IO_NO_PUNCH_HOLE:
      return Status_IO::IO_NO_PUNCH_HOLE;
    case DB_UNSUPPORTED:
      return Status_IO::UNSUPPORTED;
    case DB_IO_ERROR:
      return Status_IO::IO_ERROR;
    default:
      ut_error;
  }
}

Tablespace_node_handle::Status_IO Tablespace_node_handle::read_page(
    IORequest req, byte *buffer, page_no_t page_no) {
  const auto offset = uint64_t{page_no} * m_physical_page_size;

  const auto res = os_file_read(req, m_file_name.c_str(), m_handle, buffer,
                                offset, m_physical_page_size);

  return map_db_err_to_status_io(res);
}

#ifndef UNIV_HOTBACKUP

Tablespace_node_handle::Status_IO Tablespace_node_handle::read_page_async(
    IORequest req, byte *buffer, page_no_t page_no, Callback callback) {
  const auto offset = uint64_t{page_no} * m_physical_page_size;

  if (!recv_recovery_is_on()) {
    callback = [this, req, offset, buffer, page_no,
                original_callback = std::move(callback)](dberr_t io_result) {
      retry_read_a_few_times_if_page_id_seems_wrong(req, offset, buffer,
                                                    page_no, io_result);
      return original_callback(io_result);
    };
  }

  const auto res =
      os_aio(req, req.is_ibuf() ? AIO_mode::IBUF : AIO_mode::NORMAL,
             m_file_name.c_str(), m_handle, buffer, offset,
             m_physical_page_size, callback);

  return map_db_err_to_status_io(res);
}

#endif /* !UNIV_HOTBACKUP */

Tablespace_node_handle::Status_IO Tablespace_node_handle::write_page(
    IORequest req, byte *buffer, size_t buffer_len, page_no_t page_no) {
  const auto offset = uint64_t{page_no} * m_physical_page_size;
  const auto original_len = m_physical_page_size;

  ut_a(buffer_len <= original_len);
  req.set_original_size(original_len);

  const auto res = os_file_write(req, m_file_name.c_str(), m_handle, buffer,
                                 offset, buffer_len);

  return map_db_err_to_status_io(res);
}

#ifdef UNIV_LINUX
Tablespace_node_handle::Status_IO Tablespace_node_handle::write_pages(
    std::span<const byte *> buffers, page_no_t first_page_number) {
  const auto n_pages = buffers.size();
  ut_ad_le(n_pages, FSP_EXTENT_SIZE);

  /* Extent size is typically 64 pages except the cases where innodb page size
  is 8 KB or 4 KB, where it would be 128 pages and 256 pages respectively.
  Following is an optimization assuming in most of the cases extent size would
  be 64 pages. */
  std::array<struct iovec, 64> pre_allocated_iov;
  std::vector<struct iovec> overflow_iov;
  struct iovec *iov = nullptr;

  if (n_pages <= pre_allocated_iov.size()) {
    iov = pre_allocated_iov.data();
  } else {
    overflow_iov.resize(n_pages);
    iov = overflow_iov.data();
  }

  for (size_t i = 0; i < n_pages; i++) {
    iov[i].iov_base = const_cast<byte *>(buffers[i]);
    ut_ad(iov[i].iov_base != nullptr);
    iov[i].iov_len = m_physical_page_size;
    ut_ad(!ut::is_zeros(iov[i].iov_base, iov[i].iov_len));
  }

  dberr_t err = DB_SUCCESS;
  const ssize_t req_bytes = n_pages * m_physical_page_size;
  const auto offset = first_page_number * m_physical_page_size;
  ssize_t n = pwritev(m_handle.m_file, iov, n_pages, offset);
  if (n != req_bytes) {
    ib::error(ER_INNODB_VIO_WRITE_FAILED, m_file_name.c_str(), strerror(errno),
              req_bytes, n);
    err = DB_IO_ERROR;
  }
  ut_ad_eq(n, req_bytes);
  return map_db_err_to_status_io(err);
}
#endif /* UNIV_LINUX */

#ifndef UNIV_HOTBACKUP

Tablespace_node_handle::Status_IO Tablespace_node_handle::write_page_async(
    IORequest req, byte *buffer, size_t buffer_len, page_no_t page_no,
    Callback callback) {
  const auto offset = uint64_t{page_no} * m_physical_page_size;
  const auto original_len = m_physical_page_size;

  ut_a(buffer_len <= original_len);
  req.set_original_size(original_len);

  auto res = os_aio(req, AIO_mode::NORMAL, m_file_name.c_str(), m_handle,
                    buffer, offset, buffer_len, callback);

  return map_db_err_to_status_io(res);
}

void Tablespace_node_handle::retry_read_a_few_times_if_page_id_seems_wrong(
    IORequest req, uint64_t offset, byte *buffer, page_no_t page_no,
    dberr_t &io_result) {
  const size_t MAX_RETRIES = 10;
  for (size_t i = 0; i < MAX_RETRIES && io_result == DB_SUCCESS; ++i) {
    page_no_t read_page_no = mach_read_from_4(buffer + FIL_PAGE_OFFSET);
    space_id_t read_space_id = mach_read_from_4(buffer + FIL_PAGE_SPACE_ID);
    bool is_zero = read_page_no == 0 && read_space_id == 0;
    bool page_id_is_not_incorrect = m_node_order > 0 || read_page_no == page_no;
    /* The old InnoDB system tablespace pages could have stored a garbage
      value in the space ID field. */
    bool space_id_is_not_incorrect =
        m_space_id == 0 || read_space_id == m_space_id;
    /* We can have a zeroed page read in, it is fine and we should not complain.
     */
    if (is_zero || (page_id_is_not_incorrect && space_id_is_not_incorrect)) {
      if (i != 0) {
        ib::warn(ER_IB_FIXED_PAGE_ID, (size_t)m_space_id, (size_t)page_no,
                 (size_t)read_space_id, (size_t)read_page_no,
                 m_file_name.c_str(), i);
      }
      break;
    }
    if (i == 0) {
      ib::warn(ER_IB_WRONG_PAGE_ID, (size_t)m_space_id, (size_t)page_no,
               (size_t)read_space_id, (size_t)read_page_no,
               m_file_name.c_str());
    } else {
      /* The synchronous read operation reports success.  But did it really
      fetch the page from disk? Since the page_id is still wrong, retry. */
      ib::warn(ER_IB_WRONG_PAGEID_AFTER_SYNC_READ, (size_t)m_space_id,
               (size_t)page_no, (size_t)read_space_id, (size_t)read_page_no,
               m_file_name.c_str(), i);
      /* Before retrying the synchronous read, sleep. */
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    io_result = os_file_read(req, m_file_name.c_str(), m_handle, buffer, offset,
                             m_physical_page_size);
    if (io_result != DB_SUCCESS) {
      /* Synchronous read failed for the page. */
      ib::warn(ER_IB_SYNC_READ_FAILED, (size_t)m_space_id, (size_t)page_no,
               m_file_name.c_str(), (size_t)io_result, i);
    }
  }
}

#endif /* !UNIV_HOTBACKUP */

} /* namespace ib::fil */
