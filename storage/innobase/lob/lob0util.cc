/*****************************************************************************

Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

#include "btr0btr.h"
#include "buf0buf.h"
#include "dict0dict.h"
#include "lock0lock.h"
#include "trx0trx.h"

namespace lob {

/** Allocate one LOB page.
@param[in]	index	the index in which LOB exists.
@param[in]	lob_mtr	the mini-transaction context.
@param[in]	hint	the hint page number for allocation.
@param[in]	bulk	true if operation is OPCODE_INSERT_BULK,
                        false otherwise.
@return the allocated block of the BLOB page. */
buf_block_t *alloc_lob_page(dict_index_t *index, mtr_t *lob_mtr, page_no_t hint,
                            bool bulk) {
  ulint r_extents;
  mtr_t mtr_bulk;
  mtr_t *alloc_mtr;
  buf_block_t *block = nullptr;

  space_id_t space_id = dict_index_get_space(index);

  ut_ad(fsp_check_tablespace_size(space_id));

  if (bulk) {
    mtr_start(&mtr_bulk);
    alloc_mtr = &mtr_bulk;
  } else {
    alloc_mtr = lob_mtr;
  }

  if (!fsp_reserve_free_extents(&r_extents, space_id, 1, FSP_BLOB, alloc_mtr,
                                1)) {
    alloc_mtr->commit();
    return (nullptr);
  }

  block = btr_page_alloc(index, hint, FSP_NO_DIR, 0, alloc_mtr, lob_mtr);

  fil_space_release_free_extents(space_id, r_extents);

  if (bulk) {
    alloc_mtr->commit();
  }

  return (block);
}

}; /* namespace lob */
