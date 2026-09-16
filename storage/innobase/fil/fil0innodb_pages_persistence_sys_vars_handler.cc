/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "fil0innodb_pages_persistence_sys_vars_handler.h"

#include "buf0dblwr.h" /* dblwr::Force_crash */
#include "buf0flu.h"   /* buf_flush_sync_all_buf_pools */
#include "log0chkp.h"  /* log_checkpointing, log_checkpointer_mutex_enter */
#include "mtr0log.h"   /* mlog_write_ulint */
#include "srv0srv.h"   /* srv_checkpoint_disabled */

namespace {
#ifdef UNIV_DEBUG
void make_page_dirty(uint64_t space_id, uint32_t page_no) {
  fil_space_t *space = fil_space_acquire_silent(space_id);

  if (space == nullptr) {
    return;
  }

  if (space->m_size_in_pages <= page_no) {
    fil_space_release(space);
    return;
  }

  const auto page_id = page_id_t{space->id, page_no};

  mtr_t mtr;
  mtr.start();

  const buf_block_t *block = buf_page_get(page_id, page_size_t(space->flags),
                                          RW_X_LATCH, UT_LOCATION_HERE, &mtr);

  if (block != nullptr) {
    byte *page = block->frame;
    const page_type_t page_type = fil_page_get_type(page);

    /* Don't dirty a page that is not yet used. */
    if (page_type != FIL_PAGE_TYPE_ALLOCATED) {
      ib::info(ER_IB_MSG_574)
          << "Dirtying page: " << page_id
          << ", page_type=" << fil_get_page_type_str(page_type);

      dblwr::Force_crash = page_id;

      mlog_write_ulint(page + FIL_PAGE_TYPE, page_type, MLOG_2BYTES, &mtr);
    }
  }

  mtr.commit();

  fil_space_release(space);

  if (block != nullptr) {
    buf_flush_sync_all_buf_pools();
  }
}
#endif /* UNIV_DEBUG */
}  // namespace
namespace ib::fil {

bool Pages_persistence_sys_vars_handler::update_var(THD * /*thd */,
                                                    std::string_view name,
                                                    uint64_t new_value) {
  if (name == "innodb_adaptive_flushing_lwm") {
    srv_adaptive_flushing_lwm = new_value;
    return true;
  }

  if (name == "innodb_flushing_avg_loops") {
    srv_flushing_avg_loops = new_value;
    return true;
  }

  if (name == "innodb_idle_flush_pct") {
    srv_idle_flush_pct = new_value;
    return true;
  }

#ifdef UNIV_DEBUG
  if (name == "innodb_saved_page_number_debug") {
    srv_saved_page_number_debug = new_value;
    ib::info(ER_IB_MSG_1257)
        << "Saving InnoDB page number: " << srv_saved_page_number_debug;
    return true;
  }

  if (name == "innodb_fil_make_page_dirty_debug") {
    make_page_dirty(new_value,
                    static_cast<uint32_t>(srv_saved_page_number_debug));
    return true;
  }
#endif /* UNIV_DEBUG */
  return false;
}

bool Pages_persistence_sys_vars_handler::update_var(THD * /*thd*/,
                                                    std::string_view name,
                                                    bool new_value) {
  if (name == "innodb_adaptive_flushing") {
    srv_adaptive_flushing = new_value;
    return true;
  }

  if (name == "innodb_flush_sync") {
    srv_flush_sync = new_value;
    return true;
  }

#ifdef UNIV_DEBUG
  if (name == "innodb_log_checkpoint_fuzzy_now") {
    if (new_value && !srv_checkpoint_disabled) {
      /* Note that it's defined only when UNIV_DEBUG is defined.
      It seems to be very risky feature. Fortunately it is used
      only inside mtr tests. */
      ut_a(log_checkpointing != nullptr);
      log_checkpointing->request_fuzzy_checkpoint(true);
    }
    return true;
  }

  if (name == "innodb_checkpoint_disabled") {
    /* We need to acquire the checkpointer_mutex, to ensure that after we have
    finished this function, there will be no new checkpoint written (e.g. in
    case there is currently curring checkpoint). When checkpoint is being
    written, the same mutex is acquired, current value of
    srv_checkpoint_disabled is checked, and if checkpoints are disabled, we
    cancel writing the checkpoint. */

    log_checkpointer_mutex_enter();
    log_limits_mutex_enter();

    srv_checkpoint_disabled = new_value;

    log_limits_mutex_exit();
    log_checkpointer_mutex_exit();
    return true;
  }

  if (name == "innodb_buf_flush_list_now") {
    if (new_value) {
      buf_flush_sync_all_buf_pools();
    }
    return true;
  }
#endif /* UNIV_DEBUG */
  return false;
}

} /* namespace ib::fil */
