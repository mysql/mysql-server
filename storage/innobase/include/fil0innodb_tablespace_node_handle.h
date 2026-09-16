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

#pragma once

#include <span>
#include "fil0tablespace_node_handle_interface.h"
namespace ib::fil {

class Tablespace_node_handle final : public Tablespace_node_handle_interface {
 public:
  Tablespace_node_handle(pfs_os_file_t handle, const std::string &file_name,
                         space_id_t space_id, size_t page_size,
                         size_t node_order);

  ~Tablespace_node_handle() override;

  [[nodiscard]] bool needs_flushes_for_durability() const override;

  [[nodiscard]] Status flush() override;

  [[nodiscard]] Status truncate(Page_number size) override;

  [[nodiscard]] Status fill_range_with_zeros(Page_number first_page,
                                             Page_number number_of_pages,
                                             bool optimize_writes) override;

  [[nodiscard]] Status_IO read_page(IORequest req, byte *buffer,
                                    Page_number page_no) override;

#ifndef UNIV_HOTBACKUP
  [[nodiscard]] Status_IO read_page_async(IORequest req, byte *buffer,
                                          Page_number page_no,
                                          Callback callback) override;
#endif /* !UNIV_HOTBACKUP */

  [[nodiscard]] Status_IO write_page(IORequest req, byte *buffer,
                                     size_t buffer_len,
                                     Page_number page_no) override;

#ifdef UNIV_LINUX
  [[nodiscard]] Status_IO write_pages(std::span<const byte *> buffers,
                                      Page_number first_page_number) override;
#endif /* UNIV_LINUX */

#ifndef UNIV_HOTBACKUP
  [[nodiscard]] Status_IO write_page_async(IORequest req, byte *buffer,
                                           size_t buffer_len,
                                           Page_number page_no,
                                           Callback callback) override;
#endif /* !UNIV_HOTBACKUP */

 private:
#ifndef UNIV_HOTBACKUP
  /** Checks if the page read by a successful asynchronous read contains the
  space and page IDs that were meant to be read and in case they are not, try a
  few times to read them synchronously from disk.
  @param[in]     req        IO request.
  @param[in]     offset     An offset in bytes in the tablespace the read was
                            requested for.
  @param[out]    buffer     A buffer where to read the data in.
  @param[in]     page_no    Offset from the first page in the node to read
                            from.
  @param[in,out] io_result  Reference to the error code of the IO operation, it
                            will be have the value changed to result of last
                            read IO operation executed. */
  void retry_read_a_few_times_if_page_id_seems_wrong(IORequest req,
                                                     uint64_t offset,
                                                     byte *buffer,
                                                     page_no_t page_no,
                                                     dberr_t &io_result);
#endif /* !UNIV_HOTBACKUP */
  /** Maps DB error code to Status_IO error code. */
  [[nodiscard]] static Status_IO map_db_err_to_status_io(dberr_t err);

  /** Opened tablespace file handle. */
  const pfs_os_file_t m_handle;

  /** Tablespace file name. */
  const std::string m_file_name;

  /** Tablespace ID. */
  const space_id_t m_space_id;

  /** Physical size of a page. Needed to calculate the file offset for I/O. */
  const size_t m_physical_page_size;

  /** Number of the node in the tablespace. */
  const size_t m_node_order;
};

} /* namespace ib::fil */
