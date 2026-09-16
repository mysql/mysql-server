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

#include "fil0pages_persistence_interface.h"

#ifndef UNIV_HOTBACKUP
class MetadataRecover;

namespace ib::fil {
class Pages_persistence : public Pages_persistence_interface {
 public:
  void redo_create_tablespace(
      Tablespaces_nodes_interface::Tablespace_id /* space_id */,
      uint32_t /* flags */, const char * /* path */) override {
    /* NO OP */
  }

  void redo_delete_tablespace(
      Tablespaces_nodes_interface::Tablespace_id /* space_id */,
      const char * /* path */) override {
    /* NO OP */
  }

  void redo_rename_tablespace(Tablespace_id /* space_id */,
                              const char * /* old_path */,
                              const char * /* new_path */) override {
    /* NO OP */
  }

  [[nodiscard]] Status init() override;
  [[nodiscard]] Status assume_checkpoint_lsn(Lsn min_needed_lsn) override;
  void enable_checkpointing() override;
  void enable_periodical_checkpoints() override;
  void disable_checkpointing() override;
  void deinit() override;

  void mtr_has_dirtied_pages(
      Lsn start_lsn, Lsn end_lsn, ::Flush_observer *observer,
      ut::Function_reference<void(ut::Function_reference<void(buf_block_t *)>)>
          iterate_over_dirty_pages) override;
  void page_became_dirty(struct buf_block_t *buf_block) override;
  void persist_tablespace(Tablespace_id space_id, const trx_t *trx) override;
  void persist_tablespaces(::Flush_observer *observer) override;

  [[nodiscard]] ut::Expected<
      std::vector<Tablespaces_nodes_interface::Tablespace_id>, Status>
  recover_pages(Lsn &clean_shutdown_lsn) override;

  [[nodiscard]] Status recover_tables() override;

  void page_is_to_be_evicted(Tablespace_id space_id, Page_number page_no,
                             Lsn modification_lsn) override;

  [[nodiscard]] Lsn get_checkpoint_lsn() const override;
  void request_sharp_checkpoint() override;

  [[nodiscard]] ib::Sys_var_handler_interface &config_handler() override;

  [[nodiscard]] ib::Monitoring_interface &get_monitoring() override;

 private:
  /** PTM information gathered during recover_pages() to be applied during
  recover_tables() */
  MetadataRecover *m_dict_metadata{};

  /** Scan on disk tablespace files in recover() which is called during server
  bootstrap. */
  [[nodiscard]] dberr_t scan_tablespaces();
};

}  // namespace ib::fil

#endif /* !UNIV_HOTBACKUP */
