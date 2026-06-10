/*****************************************************************************

Copyright (c) 1996, 2026, Oracle and/or its affiliates.

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
#include "dict0upgrade.h"

#include "buf0buf.h"
#include "dict0dd.h"
#include "fil0fil.h"
#include "fsp0types.h"
#include "mtr0log.h"

/** Add server and space version number to tablespace while upgrading.
@param[in]      space_id                space id of tablespace
@param[in]      server_version_only     leave space version unchanged
@return false on success, true on failure. */
bool upgrade_space_version(const uint32_t space_id, bool server_version_only) {
  buf_block_t *block;
  page_t *page;
  mtr_t mtr;

  fil_space_t *space = fil_space_acquire_silent(space_id);

  if (space == nullptr) {
    return (true);
  }

  const page_size_t page_size(space->flags);

  mtr_start(&mtr);

  /* No logging for temporary tablespace. */
  if (fsp_is_system_temporary(space_id)) {
    mtr.set_log_mode(MTR_LOG_NO_REDO);
  }

  block = buf_page_get(page_id_t(space_id, 0), page_size, RW_SX_LATCH,
                       UT_LOCATION_HERE, &mtr);

  page = buf_block_get_frame(block);

  mlog_write_ulint(page + FIL_PAGE_SRV_VERSION, DD_SPACE_CURRENT_SRV_VERSION,
                   MLOG_4BYTES, &mtr);
  if (!server_version_only) {
    mlog_write_ulint(page + FIL_PAGE_SPACE_VERSION,
                     DD_SPACE_CURRENT_SPACE_VERSION, MLOG_4BYTES, &mtr);
  }

  mtr_commit(&mtr);
  fil_space_release(space);
  return (false);
}
