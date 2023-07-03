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

/** @file include/fut0fut.h
 File-based utilities

 Created 12/13/1995 Heikki Tuuri
 ***********************************************************************/

#ifndef fut0fut_h
#define fut0fut_h

#include "univ.i"

#include "fil0fil.h"
#include "mtr0mtr.h"

#ifndef UNIV_HOTBACKUP
/** Gets a pointer to a file address and latches the page.
@param[in]      space           Space id
@param[in]      page_size       Page size
@param[in]      addr            File address
@param[in]      rw_latch        RW_S_LATCH, RW_X_LATCH, RW_SX_LATCH
@param[out]     ptr_block       File page
@param[in,out]  mtr             Mini-transaction
@return pointer to a byte in (*ptr_block)->frame; the *ptr_block is
bufferfixed and latched */
[[nodiscard]] static inline byte *fut_get_ptr(
    space_id_t space, const page_size_t &page_size, fil_addr_t addr,
    rw_lock_type_t rw_latch, mtr_t *mtr, buf_block_t **ptr_block = nullptr);

#include "fut0fut.ic"
#endif /* !UNIV_HOTBACKUP */

#endif /* fut0fut_h */
