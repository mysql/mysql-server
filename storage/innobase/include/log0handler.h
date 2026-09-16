/*****************************************************************************

Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#pragma once

#include "log0handler_interface.h"
#include "log0sys_var_handler.h"

namespace ib::redo {

/** The type of metadata that could be handled by the handler */
enum Metadata_key : uint16_t { HEADER = 0, CHECKPOINT };

#ifndef UNIV_HOTBACKUP
/* The Redo Log Handler that provides the file based implementation of
 Handler Interface. It is the default handler used in InnoDB */
class Handler final : public Handler_interface {
 public:
  ~Handler() override;

  [[nodiscard]] Lsn align_down_to_known_boundary(Lsn lsn) override;

  [[nodiscard]] Capabilities get_capabilities() override;

  [[nodiscard]] Status create(Lsn start_lsn) override;

  [[nodiscard]] bool reconfigure(size_t max_threads,
                                 size_t reserved_bytes_per_thread) override;

  [[nodiscard]] Capacity_estimate get_capacity_estimate() override;

  void wait_for_space() override;

  [[nodiscard]] bool has_space() override;

  [[nodiscard]] Status start_writing(Lsn lsn) override;

  void stop_writing() override;

  [[nodiscard]] Status start_reading() override;

  [[nodiscard]] Status write_mtr(const Const_buffers &mtr_data, Lsn &start_lsn,
                                 Lsn &end_lsn) override;

  [[nodiscard]] Status persist_smaller_than(Lsn end_lsn,
                                            Durability desired_guarantee,
                                            Origin origin) override;

  [[nodiscard]] Status persist_available(const Origin &origin) override;

  [[nodiscard]] Status store_metadata(uint16_t key,
                                      const Metadata_value &value) override;

  [[nodiscard]] Status read(Lsn start_lsn, Buffer &buffer) override;

  [[nodiscard]] Status do_not_need_smaller_than(Lsn needed_lsn) override;
  [[nodiscard]] Status get_metadata(uint16_t key,
                                    Metadata_value &metadata) override;

  [[nodiscard]] Lsn peek_first_unassigned_lsn() override;

  [[nodiscard]] Lsn peek_first_nonpersisted_lsn() override;

  [[nodiscard]] Lsn compute_end_lsn(Lsn start_lsn,
                                    size_t data_len) const override;

  [[nodiscard]] Sys_var_handler_interface &config_handler() override;

 private:
  friend class Sys_var_handler;
  Sys_var_handler m_sys_var_handler{*this};

  /** The value of max_threads most recently passed to reconfigure */
  size_t m_max_threads{};

  /** The value of reserved_bytes_per_thread  most recently passed to
  reconfigure */
  size_t m_reserved_bytes_per_thread{};

  /** Recomputes log_sys->m_free_check_limit_lsn. It is private, but used in
  Sys_var_handler whenever innodb_redo_log_capacity changes, to learn if the
  configured capacity and number of threads make the margin safe. For similar
  reason called from `reconfigure(..)`, on startup and when
  innodb_thread_concurrency changes. But, most of the calls come from
  do_not_need_smaller_than(x), which is called whenever a checkpoint lsn is
  bumped or Log Files Governor asks Log_checkpointing to update_limits - this
  way this Log Handler implementation learns what's the oldest needed lsn, and
  can bump the m_free_check_limit_lsn accordingly.
  @return true iff log_concurrency_margin thinks the margin is safe */
  [[nodiscard]] bool update_free_check_limit();
};

[[nodiscard]] inline Handler_interface::Capabilities
Handler::get_capabilities() {
  Handler_interface::Capabilities caps;
  caps.atomic_write = false;
  caps.supports_clone = true;
  caps.supports_meb = true;
  caps.supports_disabling = true;
  caps.supports_encryption = true;
  return caps;
}

#endif /* !UNIV_HOTBACKUP */
}  // namespace ib::redo

#include "univ.i"

/* Following method should be removed as it breaks the API Interface
abstraction. But needed as of now because we allow REDO block checksum to be
disabled. Once we remove this flexibility, we can remove following functions. */

/* update the value of last_block_first_rec_group if needed */
void log_track_changes_of_recovered_lsn(
    ib::redo::Lsn old_recovered_lsn, ib::redo::Lsn new_recovered_lsn,
    ib::redo::Lsn &last_block_first_mtr_boundary);
