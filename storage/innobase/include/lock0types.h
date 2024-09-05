/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.

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

/** @file include/lock0types.h
 The transaction lock system global types

 Created 5/7/1996 Heikki Tuuri
 *******************************************************/

#include "univ.i"

#ifndef lock0types_h
#define lock0types_h

#define lock_t ib_lock_t

#include "trx0types.h"

struct lock_t;
struct lock_sys_t;
struct lock_table_t;

enum select_mode {
  SELECT_ORDINARY,    /* default behaviour */
  SELECT_SKIP_LOCKED, /* skip the row if row is locked */
  SELECT_NOWAIT       /* return immediately if row is locked */
};

/* Basic lock modes */
enum lock_mode {
  LOCK_IS = 0,          /* intention shared */
  LOCK_IX,              /* intention exclusive */
  LOCK_S,               /* shared */
  LOCK_X,               /* exclusive */
  LOCK_AUTO_INC,        /* locks the auto-inc counter of a table
                        in an exclusive mode */
  LOCK_NONE,            /* this is used elsewhere to note consistent read */
  LOCK_NUM = LOCK_NONE, /* number of lock modes */
  LOCK_NONE_UNSET = 255
};

/** Convert the given enum value into string.
@param[in]      mode    the lock mode
@return human readable string of the given enum value */
inline const char *lock_mode_string(enum lock_mode mode) {
  switch (mode) {
    case LOCK_IS:
      return ("LOCK_IS");
    case LOCK_IX:
      return ("LOCK_IX");
    case LOCK_S:
      return ("LOCK_S");
    case LOCK_X:
      return ("LOCK_X");
    case LOCK_AUTO_INC:
      return ("LOCK_AUTO_INC");
    case LOCK_NONE:
      return ("LOCK_NONE");
    case LOCK_NONE_UNSET:
      return ("LOCK_NONE_UNSET");
    default:
      ut_error;
  }
}
typedef UT_LIST_BASE_NODE_T_EXTERN(lock_t, trx_locks) trx_lock_list_t;

typedef uint32_t trx_schedule_weight_t;

/** Used to represent locks requests uniquely over time.
Please note that in case of LOCK_REC there might be actually multiple
"sub-requests" for many different heap_no associated with the same lock_t object
which we ignore here and use same guid for all of them.
Also, the lock mode and status and other properties of the lock can change over
time and/or represent more than one actual request. This is also ignored here.
What we try to achieve is some way to identify lock_t struct such that:
(1) we can serialize and deserialize it
(2) we don't have to be afraid of dangling pointers
(3) if the same lock_t gets deallocated and then allocated again, or otherwise
reused by a different transaction, we will have a different guid for it. Note,
that a single transaction never dealloactes a lock_t and allocates it again, as
all deallocations happen during commit (lock_trx_release_locks). */
struct lock_guid_t {
  /** The guid of lock->trx. Used to identify ABA problems when the same lock_t
  struct gets reused by a new transaction. */
  trx_guid_t m_trx_guid{};

  /** Id of the lock_t struct such that it does not change over time, and two
  different lock_t structs never have the same id. However it may happen that
  two "different" locks at different points in time actually reuse the same
  lock_t struct and thus have the same immutable id - this is why we also store
  the transaction's guid */
  uint64_t m_immutable_id{};

  /** Initializes the lock_guid_t object to a value which doesn't match any real
  lock. */
  lock_guid_t() = default;

  /** Initializes lock_guid_t with data uniquely identifying the lock request(s)
  represented by lock_t object.
  @param[in]  lock   the object representing the lock request(s) */
  lock_guid_t(const lock_t &lock);

  /** Checks if two guids represent the same lock (conceptually):
  they represent the same lock_t struct in memory and it was not reused.
  @param[in]  rhs   another guid to compare against
  @return true iff the two guids are equal and thus represent same lock*/
  bool operator==(const lock_guid_t &rhs) const {
    return m_trx_guid == rhs.m_trx_guid && m_immutable_id == rhs.m_immutable_id;
  }

  /** Checks if two guids represent two different locks (conceptually):
  they represent two different lock_t structs in memory or struct was reused.
  @param[in]  rhs   another guid to compare against
  @return true iff the two guids are different and thus represent different
  locks */
  bool operator!=(const lock_guid_t &rhs) const { return !(*this == rhs); }

  /** Checks if the instance is non-empty, i.e. was not default-constructed,
  but rather initialized to correspond to a real lock_t.
  @return true iff this guid was initialized to match a real lock*/
  operator bool() const { return m_immutable_id != 0; }
};
#endif /* lock0types_h */
