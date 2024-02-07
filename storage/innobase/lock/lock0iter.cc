/*****************************************************************************

Copyright (c) 2007, 2024, Oracle and/or its affiliates.

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

/** @file lock/lock0iter.cc
 Lock queue iterator. Can iterate over table and record
 lock queues.

 Created July 16, 2007 Vasil Dimov
 *******************************************************/

#define LOCK_MODULE_IMPLEMENTATION

#include <stddef.h>

#include "dict0mem.h"
#include "lock0iter.h"
#include "lock0lock.h"
#include "lock0priv.h"
#include "univ.i"

/** Initialize lock queue iterator so that it starts to iterate from
 "lock". bit_no specifies the record number within the heap where the
 record is stored. It can be undefined (ULINT_UNDEFINED) in two cases:
 1. If the lock is a table lock, thus we have a table lock queue;
 2. If the lock is a record lock and it is a wait lock. In this case
    bit_no is calculated in this function by using
    lock_rec_find_set_bit(). There is exactly one bit set in the bitmap
    of a wait lock. */
void lock_queue_iterator_reset(
    lock_queue_iterator_t *iter, /*!< out: iterator */
    const lock_t *lock,          /*!< in: lock to start from */
    ulint bit_no)                /*!< in: record number in the
                                 heap */
{
  ut_ad(lock != nullptr);
  ut_ad(locksys::owns_lock_shard(lock));

  iter->current_lock = lock;

  if (bit_no != ULINT_UNDEFINED) {
    iter->bit_no = bit_no;
  } else {
    switch (lock_get_type_low(lock)) {
      case LOCK_TABLE:
        iter->bit_no = ULINT_UNDEFINED;
        break;
      case LOCK_REC:
        iter->bit_no = lock_rec_find_set_bit(lock);
        ut_a(iter->bit_no != ULINT_UNDEFINED);
        break;
      default:
        ut_error;
    }
  }
}

/** Gets the previous lock in the lock queue, returns NULL if there are no
 more locks (i.e. the current lock is the first one). The iterator is
 receded (if not-NULL is returned).
 @return previous lock or NULL */
const lock_t *lock_queue_iterator_get_prev(
    lock_queue_iterator_t *iter) /*!< in/out: iterator */
{
  const lock_t *prev_lock;

  ut_ad(iter->current_lock != nullptr);
  ut_ad(locksys::owns_lock_shard(iter->current_lock));

  switch (lock_get_type_low(iter->current_lock)) {
    case LOCK_REC:
      prev_lock = lock_rec_get_prev(iter->current_lock, iter->bit_no);
      break;
    case LOCK_TABLE:
      prev_lock = UT_LIST_GET_PREV(tab_lock.locks, iter->current_lock);
      break;
    default:
      ut_error;
  }

  if (prev_lock != nullptr) {
    iter->current_lock = prev_lock;
  }

  return (prev_lock);
}
