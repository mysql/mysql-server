/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.

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

/** @file fut/fut0lst.cc
 File-based list utilities

 Created 11/28/1995 Heikki Tuuri
 ***********************************************************************/

#include "fut0lst.h"

#include "buf0buf.h"

#include "page0page.h"

/** Adds a node to an empty list. */
static void flst_add_to_empty(flst_base_node_t *base, /*!< in: pointer to base
                                                      node of empty list */
                              flst_node_t *node,      /*!< in: node to add */
                              mtr_t *mtr) /*!< in: mini-transaction handle */
{
  space_id_t space;
  fil_addr_t node_addr;
  ulint len;

  ut_ad(mtr && base && node);
  ut_ad(base != node);
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, base, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, node, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  len = flst_get_len(base);
  ut_a(len == 0);

  buf_ptr_get_fsp_addr(node, &space, &node_addr);

  /* Update first and last fields of base node */
  flst_write_addr(base + FLST_FIRST, node_addr, mtr);
  flst_write_addr(base + FLST_LAST, node_addr, mtr);

  /* Set prev and next fields of node to add */
  flst_write_addr(node + FLST_PREV, fil_addr_null, mtr);
  flst_write_addr(node + FLST_NEXT, fil_addr_null, mtr);

  /* Update len of base node */
  mlog_write_ulint(base + FLST_LEN, len + 1, MLOG_4BYTES, mtr);
}

/** Inserts a node after another in a list. */
void flst_insert_after(
    flst_base_node_t *base, /*!< in: pointer to base node of list */
    flst_node_t *node1,     /*!< in: node to insert after */
    flst_node_t *node2,     /*!< in: node to add */
    mtr_t *mtr);            /*!< in: mini-transaction handle */
/** Inserts a node before another in a list. */
void flst_insert_before(
    flst_base_node_t *base, /*!< in: pointer to base node of list */
    flst_node_t *node2,     /*!< in: node to insert */
    flst_node_t *node3,     /*!< in: node to insert before */
    mtr_t *mtr);            /*!< in: mini-transaction handle */

/** Adds a node as the last node in a list.
@param[in] base Pointer to base node of list
@param[in] node Node to add
@param[in] mtr Mini-transaction handle */
void flst_add_last(flst_base_node_t *base, flst_node_t *node, mtr_t *mtr) {
  space_id_t space;
  fil_addr_t node_addr;
  ulint len;
  fil_addr_t last_addr;

  ut_ad(mtr && base && node);
  ut_ad(base != node);
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, base, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, node, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  len = flst_get_len(base);
  last_addr = flst_get_last(base, mtr);

  buf_ptr_get_fsp_addr(node, &space, &node_addr);

  /* If the list is not empty, call flst_insert_after */
  if (len != 0) {
    flst_node_t *last_node;

    if (last_addr.page == node_addr.page) {
      last_node = page_align(node) + last_addr.boffset;
    } else {
      bool found;
      const page_size_t &page_size = fil_space_get_page_size(space, &found);

      ut_ad(found);

      last_node = fut_get_ptr(space, page_size, last_addr, RW_SX_LATCH, mtr);
    }

    flst_insert_after(base, last_node, node, mtr);
  } else {
    /* else call flst_add_to_empty */
    flst_add_to_empty(base, node, mtr);
  }
}

/** Adds a node as the first node in a list.
@param[in] base Pointer to base node of list
@param[in] node Node to add
@param[in] mtr Mini-transaction handle */
void flst_add_first(flst_base_node_t *base, flst_node_t *node, mtr_t *mtr) {
  space_id_t space;
  fil_addr_t node_addr;
  ulint len;
  fil_addr_t first_addr;
  flst_node_t *first_node;

  ut_ad(mtr && base && node);
  ut_ad(base != node);
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, base, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, node, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  len = flst_get_len(base);
  first_addr = flst_get_first(base, mtr);

  buf_ptr_get_fsp_addr(node, &space, &node_addr);

  /* If the list is not empty, call flst_insert_before */
  if (len != 0) {
    if (first_addr.page == node_addr.page) {
      first_node = page_align(node) + first_addr.boffset;
    } else {
      bool found;
      const page_size_t &page_size = fil_space_get_page_size(space, &found);

      ut_ad(found);

      first_node = fut_get_ptr(space, page_size, first_addr, RW_SX_LATCH, mtr);
    }

    flst_insert_before(base, node, first_node, mtr);
  } else {
    /* else call flst_add_to_empty */
    flst_add_to_empty(base, node, mtr);
  }
}

/** Inserts a node after another in a list. */
void flst_insert_after(
    flst_base_node_t *base, /*!< in: pointer to base node of list */
    flst_node_t *node1,     /*!< in: node to insert after */
    flst_node_t *node2,     /*!< in: node to add */
    mtr_t *mtr)             /*!< in: mini-transaction handle */
{
  space_id_t space;
  fil_addr_t node1_addr;
  fil_addr_t node2_addr;
  flst_node_t *node3;
  fil_addr_t node3_addr;
  ulint len;

  ut_ad(mtr && node1 && node2 && base);
  ut_ad(base != node1);
  ut_ad(base != node2);
  ut_ad(node2 != node1);
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, base, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, node1, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, node2, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));

  buf_ptr_get_fsp_addr(node1, &space, &node1_addr);
  buf_ptr_get_fsp_addr(node2, &space, &node2_addr);

  node3_addr = flst_get_next_addr(node1, mtr);

  /* Set prev and next fields of node2 */
  flst_write_addr(node2 + FLST_PREV, node1_addr, mtr);
  flst_write_addr(node2 + FLST_NEXT, node3_addr, mtr);

  if (!fil_addr_is_null(node3_addr)) {
    /* Update prev field of node3 */
    bool found;
    const page_size_t &page_size = fil_space_get_page_size(space, &found);

    ut_ad(found);

    node3 = fut_get_ptr(space, page_size, node3_addr, RW_SX_LATCH, mtr);
    flst_write_addr(node3 + FLST_PREV, node2_addr, mtr);
  } else {
    /* node1 was last in list: update last field in base */
    flst_write_addr(base + FLST_LAST, node2_addr, mtr);
  }

  /* Set next field of node1 */
  flst_write_addr(node1 + FLST_NEXT, node2_addr, mtr);

  /* Update len of base node */
  len = flst_get_len(base);
  mlog_write_ulint(base + FLST_LEN, len + 1, MLOG_4BYTES, mtr);
}

/** Inserts a node before another in a list. */
void flst_insert_before(
    flst_base_node_t *base, /*!< in: pointer to base node of list */
    flst_node_t *node2,     /*!< in: node to insert */
    flst_node_t *node3,     /*!< in: node to insert before */
    mtr_t *mtr)             /*!< in: mini-transaction handle */
{
  space_id_t space;
  flst_node_t *node1;
  fil_addr_t node1_addr;
  fil_addr_t node2_addr;
  fil_addr_t node3_addr;
  ulint len;

  ut_ad(mtr && node2 && node3 && base);
  ut_ad(base != node2);
  ut_ad(base != node3);
  ut_ad(node2 != node3);
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, base, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, node2, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, node3, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));

  buf_ptr_get_fsp_addr(node2, &space, &node2_addr);
  buf_ptr_get_fsp_addr(node3, &space, &node3_addr);

  node1_addr = flst_get_prev_addr(node3, mtr);

  /* Set prev and next fields of node2 */
  flst_write_addr(node2 + FLST_PREV, node1_addr, mtr);
  flst_write_addr(node2 + FLST_NEXT, node3_addr, mtr);

  if (!fil_addr_is_null(node1_addr)) {
    bool found;
    const page_size_t &page_size = fil_space_get_page_size(space, &found);

    ut_ad(found);

    /* Update next field of node1 */
    node1 = fut_get_ptr(space, page_size, node1_addr, RW_SX_LATCH, mtr);
    flst_write_addr(node1 + FLST_NEXT, node2_addr, mtr);
  } else {
    /* node3 was first in list: update first field in base */
    flst_write_addr(base + FLST_FIRST, node2_addr, mtr);
  }

  /* Set prev field of node3 */
  flst_write_addr(node3 + FLST_PREV, node2_addr, mtr);

  /* Update len of base node */
  len = flst_get_len(base);
  mlog_write_ulint(base + FLST_LEN, len + 1, MLOG_4BYTES, mtr);
}

/** Removes a node.
@param[in] base Pointer to base node of list
@param[in] node2 Node to remove
@param[in] mtr Mini-transaction handle */
void flst_remove(flst_base_node_t *base, flst_node_t *node2, mtr_t *mtr) {
  space_id_t space;
  flst_node_t *node1;
  fil_addr_t node1_addr;
  fil_addr_t node2_addr;
  flst_node_t *node3;
  fil_addr_t node3_addr;
  ulint len;

  ut_ad(mtr && node2 && base);
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, base, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, node2, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));

  buf_ptr_get_fsp_addr(node2, &space, &node2_addr);

  bool found;
  const page_size_t &page_size = fil_space_get_page_size(space, &found);

  ut_ad(found);

  node1_addr = flst_get_prev_addr(node2, mtr);
  node3_addr = flst_get_next_addr(node2, mtr);

  if (!fil_addr_is_null(node1_addr)) {
    /* Update next field of node1 */

    if (node1_addr.page == node2_addr.page) {
      node1 = page_align(node2) + node1_addr.boffset;
    } else {
      node1 = fut_get_ptr(space, page_size, node1_addr, RW_SX_LATCH, mtr);
    }

    ut_ad(node1 != node2);

    flst_write_addr(node1 + FLST_NEXT, node3_addr, mtr);
  } else {
    /* node2 was first in list: update first field in base */
    flst_write_addr(base + FLST_FIRST, node3_addr, mtr);
  }

  if (!fil_addr_is_null(node3_addr)) {
    /* Update prev field of node3 */

    if (node3_addr.page == node2_addr.page) {
      node3 = page_align(node2) + node3_addr.boffset;
    } else {
      node3 = fut_get_ptr(space, page_size, node3_addr, RW_SX_LATCH, mtr);
    }

    ut_ad(node2 != node3);

    flst_write_addr(node3 + FLST_PREV, node1_addr, mtr);
  } else {
    /* node2 was last in list: update last field in base */
    flst_write_addr(base + FLST_LAST, node1_addr, mtr);
  }

  /* Update len of base node */
  len = flst_get_len(base);
  ut_ad(len > 0);

  mlog_write_ulint(base + FLST_LEN, len - 1, MLOG_4BYTES, mtr);
}

void flst_validate(const flst_base_node_t *base, mtr_t *mtr1) {
  space_id_t space;
  const flst_node_t *node;
  fil_addr_t node_addr;
  fil_addr_t base_addr;
  ulint len;
  ulint i;
  mtr_t mtr2;

  ut_ad(base);
  ut_ad(mtr_memo_contains_page_flagged(
      mtr1, base, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));

  /* We use two mini-transaction handles: the first is used to
  lock the base node, and prevent other threads from modifying the
  list. The second is used to traverse the list. We cannot run the
  second mtr without committing it at times, because if the list
  is long, then the x-locked pages could fill the buffer resulting
  in a deadlock. */

  /* Find out the space id */
  buf_ptr_get_fsp_addr(base, &space, &base_addr);

  bool found;
  const page_size_t &page_size = fil_space_get_page_size(space, &found);

  ut_ad(found);

  len = flst_get_len(base);
  node_addr = flst_get_first(base, mtr1);

  for (i = 0; i < len; i++) {
    mtr_start(&mtr2);

    node = fut_get_ptr(space, page_size, node_addr, RW_SX_LATCH, &mtr2);
    node_addr = flst_get_next_addr(node, &mtr2);

    mtr_commit(&mtr2); /* Commit mtr2 each round to prevent buffer
                       becoming full */
  }

  ut_a(fil_addr_is_null(node_addr));

  node_addr = flst_get_last(base, mtr1);

  for (i = 0; i < len; i++) {
    mtr_start(&mtr2);

    node = fut_get_ptr(space, page_size, node_addr, RW_SX_LATCH, &mtr2);
    node_addr = flst_get_prev_addr(node, &mtr2);

    mtr_commit(&mtr2); /* Commit mtr2 each round to prevent buffer
                       becoming full */
  }

  ut_a(fil_addr_is_null(node_addr));
}
