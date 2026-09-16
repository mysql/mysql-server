/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "log0handler_interface.h"

#include "ut0dbg.h"

#ifdef UNIV_DEBUG

#include "mtr0mtr.h"
#include "sync0debug.h"
#include "sync0types.h"
#include "ut0log.h"

static void log_free_check_validate() {
  /* This function should not be called while holding any latches.
  However, for legacy reasons we permit a few exceptions, which empirically
  didn't lead to a deadlock so far. Do not add any new execptions to this
  list! Rather try to shrink it. */
  static const latch_level_t latches[] = {
      SYNC_NO_ORDER_CHECK, /* used for non-labeled latches */
      SYNC_RSEGS,          /* rsegs->x_lock in trx_rseg_create() */
      SYNC_UNDO_DDL,       /* undo::ddl_mutex */
      SYNC_UNDO_SPACES,    /* undo_truncate::spaces::m_latch */
      SYNC_FTS_CACHE,      /* fts_cache_t::lock */
      SYNC_DICT,           /* dict_sys->mutex in commit_try_rebuild() */
      SYNC_DICT_OPERATION, /* X-latch in commit_try_rebuild() */
      SYNC_INDEX_TREE      /* index->lock */
  };

  sync_allowed_latches check(std::begin(latches), std::end(latches));
  // This check only does anything when --innodb-sync-debug is on
  if (sync_check_iterate(check)) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_HOLDING_LATCHES_WAITING_FOR_SPACE);
  }
  // This check works in any debug build
  mtr_t::check_my_thread_mtrs_are_not_latching();
}
#endif /* !UNIV_DEBUG */

void log_free_check() {
  ut_d(log_free_check_validate());
  ib::redo::handler->wait_for_space();
}
