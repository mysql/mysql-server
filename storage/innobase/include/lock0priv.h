/*****************************************************************************

Copyright (c) 2007, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/lock0priv.h
 Lock module internal structures and methods.

 Created July 12, 2007 Vasil Dimov
 *******************************************************/

#ifndef lock0priv_h
#define lock0priv_h

#ifndef LOCK_MODULE_IMPLEMENTATION
/* If you need to access members of the structures defined in this
file, please write appropriate functions that retrieve them and put
those functions in lock/ */
#error Do not include lock0priv.h outside of the lock/ module
#endif

#include "dict0types.h"
#include "hash0hash.h"
#include "trx0types.h"
#include "univ.i"

#include <utility>

/** A table lock */
struct lock_table_t {
  dict_table_t *table; /*!< database table in dictionary
                       cache */
  UT_LIST_NODE_T(lock_t)
  locks; /*!< list of locks on the same
         table */
  /** Print the table lock into the given output stream
  @param[in,out]	out	the output stream
  @return the given output stream. */
  std::ostream &print(std::ostream &out) const;
};

/** Print the table lock into the given output stream
@param[in,out]	out	the output stream
@return the given output stream. */
inline std::ostream &lock_table_t::print(std::ostream &out) const {
  out << "[lock_table_t: name=" << table->name << "]";
  return (out);
}

/** The global output operator is overloaded to conveniently
print the lock_table_t object into the given output stream.
@param[in,out]	out	the output stream
@param[in]	lock	the table lock
@return the given output stream */
inline std::ostream &operator<<(std::ostream &out, const lock_table_t &lock) {
  return (lock.print(out));
}

/** Record lock for a page */
struct lock_rec_t {
  space_id_t space;  /*!< space id */
  page_no_t page_no; /*!< page number */
  uint32_t n_bits;   /*!< number of bits in the lock
                     bitmap; NOTE: the lock bitmap is
                     placed immediately after the
                     lock struct */

  /** Print the record lock into the given output stream
  @param[in,out]	out	the output stream
  @return the given output stream. */
  std::ostream &print(std::ostream &out) const;
};

/** Print the record lock into the given output stream
@param[in,out]	out	the output stream
@return the given output stream. */
inline std::ostream &lock_rec_t::print(std::ostream &out) const {
  out << "[lock_rec_t: space=" << space << ", page_no=" << page_no
      << ", n_bits=" << n_bits << "]";
  return (out);
}

inline std::ostream &operator<<(std::ostream &out, const lock_rec_t &lock) {
  return (lock.print(out));
}

/** Lock struct; protected by lock_sys->mutex */
struct lock_t {
  /** transaction owning the lock */
  trx_t *trx;

  /** list of the locks of the transaction */
  UT_LIST_NODE_T(lock_t) trx_locks;

  /** Index for a record lock */
  dict_index_t *index;

  /** Hash chain node for a record lock. The link node in a singly
  linked list, used by the hash table. */
  lock_t *hash;

  union {
    /** Table lock */
    lock_table_t tab_lock;

    /** Record lock */
    lock_rec_t rec_lock;
  };

#ifdef HAVE_PSI_THREAD_INTERFACE
#ifdef HAVE_PSI_DATA_LOCK_INTERFACE
  /** Performance schema thread that created the lock. */
  ulonglong m_psi_internal_thread_id;

  /** Performance schema event that created the lock. */
  ulonglong m_psi_event_id;
#endif /* HAVE_PSI_DATA_LOCK_INTERFACE */
#endif /* HAVE_PSI_THREAD_INTERFACE */

  /** The lock type and mode bit flags.
  LOCK_GAP or LOCK_REC_NOT_GAP, LOCK_INSERT_INTENTION, wait flag, ORed */
  uint32_t type_mode;

#if defined(UNIV_DEBUG)
  /** Timestamp when it was created. */
  uint64_t m_seq;
#endif /* UNIV_DEBUG */

  /** Remove GAP lock from a next Key Lock */
  void remove_gap_lock() {
    ut_ad(!is_gap());
    ut_ad(!is_insert_intention());
    ut_ad(is_record_lock());

    type_mode |= LOCK_REC_NOT_GAP;
  }

  /** Determine if the lock object is a record lock.
  @return true if record lock, false otherwise. */
  bool is_record_lock() const { return (type() == LOCK_REC); }

  /** Determine if it is predicate lock.
  @return true if predicate lock, false otherwise. */
  bool is_predicate() const {
    return (type_mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE));
  }

  /** @return true if the lock wait flag is set */
  bool is_waiting() const { return (type_mode & LOCK_WAIT); }

  /** @return true if the gap lock bit is set */
  bool is_gap() const { return (type_mode & LOCK_GAP); }

  /** @return true if the not gap lock bit is set */
  bool is_record_not_gap() const { return (type_mode & LOCK_REC_NOT_GAP); }

  /** @return true if the insert intention bit is set */
  bool is_insert_intention() const {
    return (type_mode & LOCK_INSERT_INTENTION);
  }

  /** @return the lock mode */
  ulint type() const { return (type_mode & LOCK_TYPE_MASK); }

  /** @return the precise lock mode */
  lock_mode mode() const {
    return (static_cast<lock_mode>(type_mode & LOCK_MODE_MASK));
  }

  /** Get lock hash table
  @return lock hash table */
  hash_table_t *hash_table() const { return (lock_hash_get(type_mode)); }

  /** @return the record lock tablespace ID */
  space_id_t space_id() const {
    ut_ad(is_record_lock());

    return (rec_lock.space);
  }

  /** @return the record lock page number */
  page_no_t page_no() const {
    ut_ad(is_record_lock());

    return (rec_lock.page_no);
  }

  /** @return the transaction's query thread state. */
  trx_que_t trx_que_state() const { return (trx->lock.que_state); }

  /** Print the lock object into the given output stream.
  @param[in,out]	out	the output stream
  @return the given output stream. */
  std::ostream &print(std::ostream &out) const;

  /** Convert the member 'type_mode' into a human readable string.
  @return human readable string */
  std::string type_mode_string() const;

  /* @return the string/text representation of the record type. */
  const char *type_string() const {
    switch (type_mode & LOCK_TYPE_MASK) {
      case LOCK_REC:
        return ("LOCK_REC");
      case LOCK_TABLE:
        return ("LOCK_TABLE");
      default:
        ut_error;
    }
  }
};

/** Convert the member 'type_mode' into a human readable string.
@return human readable string */
inline std::string lock_t::type_mode_string() const {
  std::ostringstream sout;
  sout << type_string();
  sout << " | " << lock_mode_string(mode());

  if (is_record_not_gap()) {
    sout << " | LOCK_REC_NOT_GAP";
  }

  if (is_waiting()) {
    sout << " | LOCK_WAIT";
  }

  if (is_gap()) {
    sout << " | LOCK_GAP";
  }

  if (is_insert_intention()) {
    sout << " | LOCK_INSERT_INTENTION";
  }
  return (sout.str());
}

inline std::ostream &lock_t::print(std::ostream &out) const {
  out << "[lock_t: type_mode=" << type_mode << "(" << type_mode_string() << ")";

  if (is_record_lock()) {
    out << rec_lock;
  } else {
    out << tab_lock;
  }

  out << "]";
  return (out);
}

inline std::ostream &operator<<(std::ostream &out, const lock_t &lock) {
  return (lock.print(out));
}

#ifdef UNIV_DEBUG
extern ibool lock_print_waits;
#endif /* UNIV_DEBUG */

/** Restricts the length of search we will do in the waits-for
graph of transactions */
static const ulint LOCK_MAX_N_STEPS_IN_DEADLOCK_CHECK = 1000000;

/** Restricts the search depth we will do in the waits-for graph of
transactions */
static const ulint LOCK_MAX_DEPTH_IN_DEADLOCK_CHECK = 200;

/** When releasing transaction locks, this specifies how often we release
the lock mutex for a moment to give also others access to it */
static const ulint LOCK_RELEASE_INTERVAL = 1000;

/* Safety margin when creating a new record lock: this many extra records
can be inserted to the page without need to create a lock with a bigger
bitmap */

static const ulint LOCK_PAGE_BITMAP_MARGIN = 64;

/* An explicit record lock affects both the record and the gap before it.
An implicit x-lock does not affect the gap, it only locks the index
record from read or update.

If a transaction has modified or inserted an index record, then
it owns an implicit x-lock on the record. On a secondary index record,
a transaction has an implicit x-lock also if it has modified the
clustered index record, the max trx id of the page where the secondary
index record resides is >= trx id of the transaction (or database recovery
is running), and there are no explicit non-gap lock requests on the
secondary index record.

This complicated definition for a secondary index comes from the
implementation: we want to be able to determine if a secondary index
record has an implicit x-lock, just by looking at the present clustered
index record, not at the historical versions of the record. The
complicated definition can be explained to the user so that there is
nondeterminism in the access path when a query is answered: we may,
or may not, access the clustered index record and thus may, or may not,
bump into an x-lock set there.

Different transaction can have conflicting locks set on the gap at the
same time. The locks on the gap are purely inhibitive: an insert cannot
be made, or a select cursor may have to wait if a different transaction
has a conflicting lock on the gap. An x-lock on the gap does not give
the right to insert into the gap.

An explicit lock can be placed on a user record or the supremum record of
a page. The locks on the supremum record are always thought to be of the gap
type, though the gap bit is not set. When we perform an update of a record
where the size of the record changes, we may temporarily store its explicit
locks on the infimum record of the page, though the infimum otherwise never
carries locks.

A waiting record lock can also be of the gap type. A waiting lock request
can be granted when there is no conflicting mode lock request by another
transaction ahead of it in the explicit lock queue.

In version 4.0.5 we added yet another explicit lock type: LOCK_REC_NOT_GAP.
It only locks the record it is placed on, not the gap before the record.
This lock type is necessary to emulate an Oracle-like READ COMMITTED isolation
level.

-------------------------------------------------------------------------
RULE 1: If there is an implicit x-lock on a record, and there are non-gap
-------
lock requests waiting in the queue, then the transaction holding the implicit
x-lock also has an explicit non-gap record x-lock. Therefore, as locks are
released, we can grant locks to waiting lock requests purely by looking at
the explicit lock requests in the queue.

RULE 3: Different transactions cannot have conflicting granted non-gap locks
-------
on a record at the same time. However, they can have conflicting granted gap
locks.
RULE 4: If a there is a waiting lock request in a queue, no lock request,
-------
gap or not, can be inserted ahead of it in the queue. In record deletes
and page splits new gap type locks can be created by the database manager
for a transaction, and without rule 4, the waits-for graph of transactions
might become cyclic without the database noticing it, as the deadlock check
is only performed when a transaction itself requests a lock!
-------------------------------------------------------------------------

An insert is allowed to a gap if there are no explicit lock requests by
other transactions on the next record. It does not matter if these lock
requests are granted or waiting, gap bit set or not, with the exception
that a gap type request set by another transaction to wait for
its turn to do an insert is ignored. On the other hand, an
implicit x-lock by another transaction does not prevent an insert, which
allows for more concurrency when using an Oracle-style sequence number
generator for the primary key with many transactions doing inserts
concurrently.

A modify of a record is allowed if the transaction has an x-lock on the
record, or if other transactions do not have any non-gap lock requests on the
record.

A read of a single user record with a cursor is allowed if the transaction
has a non-gap explicit, or an implicit lock on the record, or if the other
transactions have no x-lock requests on the record. At a page supremum a
read is always allowed.

In summary, an implicit lock is seen as a granted x-lock only on the
record, not on the gap. An explicit lock with no gap bit set is a lock
both on the record and the gap. If the gap bit is set, the lock is only
on the gap. Different transaction cannot own conflicting locks on the
record at the same time, but they may own conflicting locks on the gap.
Granted locks on a record give an access right to the record, but gap type
locks just inhibit operations.

NOTE: Finding out if some transaction has an implicit x-lock on a secondary
index record can be cumbersome. We may have to look at previous versions of
the corresponding clustered index record to find out if a delete marked
secondary index record was delete marked by an active transaction, not by
a committed one.

FACT A: If a transaction has inserted a row, it can delete it any time
without need to wait for locks.

PROOF: The transaction has an implicit x-lock on every index record inserted
for the row, and can thus modify each record without the need to wait. Q.E.D.

FACT B: If a transaction has read some result set with a cursor, it can read
it again, and retrieves the same result set, if it has not modified the
result set in the meantime. Hence, there is no phantom problem. If the
biggest record, in the alphabetical order, touched by the cursor is removed,
a lock wait may occur, otherwise not.

PROOF: When a read cursor proceeds, it sets an s-lock on each user record
it passes, and a gap type s-lock on each page supremum. The cursor must
wait until it has these locks granted. Then no other transaction can
have a granted x-lock on any of the user records, and therefore cannot
modify the user records. Neither can any other transaction insert into
the gaps which were passed over by the cursor. Page splits and merges,
and removal of obsolete versions of records do not affect this, because
when a user record or a page supremum is removed, the next record inherits
its locks as gap type locks, and therefore blocks inserts to the same gap.
Also, if a page supremum is inserted, it inherits its locks from the successor
record. When the cursor is positioned again at the start of the result set,
the records it will touch on its course are either records it touched
during the last pass or new inserted page supremums. It can immediately
access all these records, and when it arrives at the biggest record, it
notices that the result set is complete. If the biggest record was removed,
lock wait can occur because the next record only inherits a gap type lock,
and a wait may be needed. Q.E.D. */

/* If an index record should be changed or a new inserted, we must check
the lock on the record or the next. When a read cursor starts reading,
we will set a record level s-lock on each record it passes, except on the
initial record on which the cursor is positioned before we start to fetch
records. Our index tree search has the convention that the B-tree
cursor is positioned BEFORE the first possibly matching record in
the search. Optimizations are possible here: if the record is searched
on an equality condition to a unique key, we could actually set a special
lock on the record, a lock which would not prevent any insert before
this record. In the next key locking an x-lock set on a record also
prevents inserts just before that record.
        There are special infimum and supremum records on each page.
A supremum record can be locked by a read cursor. This records cannot be
updated but the lock prevents insert of a user record to the end of
the page.
        Next key locks will prevent the phantom problem where new rows
could appear to SELECT result sets after the select operation has been
performed. Prevention of phantoms ensures the serilizability of
transactions.
        What should we check if an insert of a new record is wanted?
Only the lock on the next record on the same page, because also the
supremum record can carry a lock. An s-lock prevents insertion, but
what about an x-lock? If it was set by a searched update, then there
is implicitly an s-lock, too, and the insert should be prevented.
What if our transaction owns an x-lock to the next record, but there is
a waiting s-lock request on the next record? If this s-lock was placed
by a read cursor moving in the ascending order in the index, we cannot
do the insert immediately, because when we finally commit our transaction,
the read cursor should see also the new inserted record. So we should
move the read cursor backward from the next record for it to pass over
the new inserted record. This move backward may be too cumbersome to
implement. If we in this situation just enqueue a second x-lock request
for our transaction on the next record, then the deadlock mechanism
notices a deadlock between our transaction and the s-lock request
transaction. This seems to be an ok solution.
        We could have the convention that granted explicit record locks,
lock the corresponding records from changing, and also lock the gaps
before them from inserting. A waiting explicit lock request locks the gap
before from inserting. Implicit record x-locks, which we derive from the
transaction id in the clustered index record, only lock the record itself
from modification, not the gap before it from inserting.
        How should we store update locks? If the search is done by a unique
key, we could just modify the record trx id. Otherwise, we could put a record
x-lock on the record. If the update changes ordering fields of the
clustered index record, the inserted new record needs no record lock in
lock table, the trx id is enough. The same holds for a secondary index
record. Searched delete is similar to update.

PROBLEM:
What about waiting lock requests? If a transaction is waiting to make an
update to a record which another modified, how does the other transaction
know to send the end-lock-wait signal to the waiting transaction? If we have
the convention that a transaction may wait for just one lock at a time, how
do we preserve it if lock wait ends?

PROBLEM:
Checking the trx id label of a secondary index record. In the case of a
modification, not an insert, is this necessary? A secondary index record
is modified only by setting or resetting its deleted flag. A secondary index
record contains fields to uniquely determine the corresponding clustered
index record. A secondary index record is therefore only modified if we
also modify the clustered index record, and the trx id checking is done
on the clustered index record, before we come to modify the secondary index
record. So, in the case of delete marking or unmarking a secondary index
record, we do not have to care about trx ids, only the locks in the lock
table must be checked. In the case of a select from a secondary index, the
trx id is relevant, and in this case we may have to search the clustered
index record.

PROBLEM: How to update record locks when page is split or merged, or
--------------------------------------------------------------------
a record is deleted or updated?
If the size of fields in a record changes, we perform the update by
a delete followed by an insert. How can we retain the locks set or
waiting on the record? Because a record lock is indexed in the bitmap
by the heap number of the record, when we remove the record from the
record list, it is possible still to keep the lock bits. If the page
is reorganized, we could make a table of old and new heap numbers,
and permute the bitmaps in the locks accordingly. We can add to the
table a row telling where the updated record ended. If the update does
not require a reorganization of the page, we can simply move the lock
bits for the updated record to the position determined by its new heap
number (we may have to allocate a new lock, if we run out of the bitmap
in the old one).
        A more complicated case is the one where the reinsertion of the
updated record is done pessimistically, because the structure of the
tree may change.

PROBLEM: If a supremum record is removed in a page merge, or a record
---------------------------------------------------------------------
removed in a purge, what to do to the waiting lock requests? In a split to
the right, we just move the lock requests to the new supremum. If a record
is removed, we could move the waiting lock request to its inheritor, the
next record in the index. But, the next record may already have lock
requests on its own queue. A new deadlock check should be made then. Maybe
it is easier just to release the waiting transactions. They can then enqueue
new lock requests on appropriate records.

PROBLEM: When a record is inserted, what locks should it inherit from the
-------------------------------------------------------------------------
upper neighbor? An insert of a new supremum record in a page split is
always possible, but an insert of a new user record requires that the upper
neighbor does not have any lock requests by other transactions, granted or
waiting, in its lock queue. Solution: We can copy the locks as gap type
locks, so that also the waiting locks are transformed to granted gap type
locks on the inserted record. */

/* LOCK COMPATIBILITY MATRIX
 *    IS IX S  X  AI
 * IS +	 +  +  -  +
 * IX +	 +  -  -  +
 * S  +	 -  +  -  -
 * X  -	 -  -  -  -
 * AI +	 +  -  -  -
 *
 * Note that for rows, InnoDB only acquires S or X locks.
 * For tables, InnoDB normally acquires IS or IX locks.
 * S or X table locks are only acquired for LOCK TABLES.
 * Auto-increment (AI) locks are needed because of
 * statement-level MySQL binlog.
 * See also lock_mode_compatible().
 */
static const byte lock_compatibility_matrix[5][5] = {
    /**         IS     IX       S     X       AI */
    /* IS */ {TRUE, TRUE, TRUE, FALSE, TRUE},
    /* IX */ {TRUE, TRUE, FALSE, FALSE, TRUE},
    /* S  */ {TRUE, FALSE, TRUE, FALSE, FALSE},
    /* X  */ {FALSE, FALSE, FALSE, FALSE, FALSE},
    /* AI */ {TRUE, TRUE, FALSE, FALSE, FALSE}};

/* STRONGER-OR-EQUAL RELATION (mode1=row, mode2=column)
 *    IS IX S  X  AI
 * IS +  -  -  -  -
 * IX +  +  -  -  -
 * S  +  -  +  -  -
 * X  +  +  +  +  +
 * AI -  -  -  -  +
 * See lock_mode_stronger_or_eq().
 */
static const byte lock_strength_matrix[5][5] = {
    /**         IS     IX       S     X       AI */
    /* IS */ {TRUE, FALSE, FALSE, FALSE, FALSE},
    /* IX */ {TRUE, TRUE, FALSE, FALSE, FALSE},
    /* S  */ {TRUE, FALSE, TRUE, FALSE, FALSE},
    /* X  */ {TRUE, TRUE, TRUE, TRUE, TRUE},
    /* AI */ {FALSE, FALSE, FALSE, FALSE, TRUE}};

/** Maximum depth of the DFS stack. */
static const ulint MAX_STACK_SIZE = 4096;

#define PRDT_HEAPNO PAGE_HEAP_NO_INFIMUM
/** Record locking request status */
enum lock_rec_req_status {
  /** Failed to acquire a lock */
  LOCK_REC_FAIL,
  /** Succeeded in acquiring a lock (implicit or already acquired) */
  LOCK_REC_SUCCESS,
  /** Explicitly created a new lock */
  LOCK_REC_SUCCESS_CREATED
};

/**
Record lock ID */
struct RecID {
  /** Constructor
  @param[in]	lock		Record lock
  @param[in]	heap_no		Heap number in the page */
  RecID(const lock_t *lock, ulint heap_no)
      : m_space_id(lock->rec_lock.space),
        m_page_no(lock->rec_lock.page_no),
        m_heap_no(static_cast<uint32_t>(heap_no)),
        m_fold(lock_rec_fold(m_space_id, m_page_no)) {
    ut_ad(m_space_id < UINT32_MAX);
    ut_ad(m_page_no < UINT32_MAX);
    ut_ad(m_heap_no < UINT32_MAX);
  }

  /** Constructor
  @param[in]	space_id	Tablespace ID
  @param[in]	page_no		Page number in space_id
  @param[in]	heap_no		Heap number in <space_id, page_no> */
  RecID(space_id_t space_id, page_no_t page_no, ulint heap_no)
      : m_space_id(space_id),
        m_page_no(page_no),
        m_heap_no(static_cast<uint32_t>(heap_no)),
        m_fold(lock_rec_fold(m_space_id, m_page_no)) {
    ut_ad(m_space_id < UINT32_MAX);
    ut_ad(m_page_no < UINT32_MAX);
    ut_ad(m_heap_no < UINT32_MAX);
  }

  /** Constructor
  @param[in]	block		Block in a tablespace
  @param[in]	heap_no		Heap number in the block */
  RecID(const buf_block_t *block, ulint heap_no)
      : m_space_id(block->page.id.space()),
        m_page_no(block->page.id.page_no()),
        m_heap_no(static_cast<uint32_t>(heap_no)),
        m_fold(lock_rec_fold(m_space_id, m_page_no)) {
    ut_ad(heap_no < UINT32_MAX);
  }

  /**
  @return the "folded" value of {space, page_no} */
  ulint fold() const { return (m_fold); }

  /** @return true if it's the supremum record */
  bool is_supremum() const { return (m_heap_no == PAGE_HEAP_NO_SUPREMUM); }

  /* Check if the rec id matches the lock instance.
  @param[i]	lock		Lock to compare with
  @return true if <space, page_no, heap_no> matches the lock. */
  inline bool matches(const lock_t *lock) const;

  /**
  Tablespace ID */
  space_id_t m_space_id;

  /**
  Page number within the space ID */
  page_no_t m_page_no;

  /**
  Heap number within the page */
  uint32_t m_heap_no;

  /**
  Hashed key value */
  ulint m_fold;
};

/**
Create record locks */
class RecLock {
 public:
  /**
  @param[in,out] thr	Transaction query thread requesting the record
                          lock
  @param[in] index	Index on which record lock requested
  @param[in] rec_id	Record lock tuple {space, page_no, heap_no}
  @param[in] mode		The lock mode */
  RecLock(que_thr_t *thr, dict_index_t *index, const RecID &rec_id, ulint mode)
      : m_thr(thr),
        m_trx(thr_get_trx(thr)),
        m_mode(mode),
        m_index(index),
        m_rec_id(rec_id) {
    ut_ad(is_predicate_lock(m_mode));

    init(NULL);
  }

  /**
  @param[in,out] thr	Transaction query thread requesting the record
                          lock
  @param[in] index	Index on which record lock requested
  @param[in] block	Buffer page containing record
  @param[in] heap_no	Heap number within the block
  @param[in] mode		The lock mode
  @param[in] prdt		The predicate for the rtree lock */
  RecLock(que_thr_t *thr, dict_index_t *index, const buf_block_t *block,
          ulint heap_no, ulint mode, lock_prdt_t *prdt = NULL)
      : m_thr(thr),
        m_trx(thr_get_trx(thr)),
        m_mode(mode),
        m_index(index),
        m_rec_id(block, heap_no) {
    btr_assert_not_corrupted(block, index);

    init(block->frame);
  }

  /**
  @param[in] index	Index on which record lock requested
  @param[in] rec_id	Record lock tuple {space, page_no, heap_no}
  @param[in] mode		The lock mode */
  RecLock(dict_index_t *index, const RecID &rec_id, ulint mode)
      : m_thr(), m_trx(), m_mode(mode), m_index(index), m_rec_id(rec_id) {
    ut_ad(is_predicate_lock(m_mode));

    init(NULL);
  }

  /**
  @param[in] index	Index on which record lock requested
  @param[in] block	Buffer page containing record
  @param[in] heap_no	Heap number withing block
  @param[in] mode		The lock mode */
  RecLock(dict_index_t *index, const buf_block_t *block, ulint heap_no,
          ulint mode)
      : m_thr(),
        m_trx(),
        m_mode(mode),
        m_index(index),
        m_rec_id(block, heap_no) {
    btr_assert_not_corrupted(block, index);

    init(block->frame);
  }

  /**
  Enqueue a lock wait for a transaction. If it is a high priority
  transaction (cannot rollback) then jump ahead in the record lock wait
  queue and if the transaction at the head of the queue is itself waiting
  roll it back.
  @param[in, out] wait_for	The lock that the the joining
                                  transaction is waiting for
  @param[in] prdt			Predicate [optional]
  @return DB_LOCK_WAIT, DB_DEADLOCK, or
          DB_SUCCESS_LOCKED_REC; DB_SUCCESS_LOCKED_REC means that
          there was a deadlock, but another transaction was chosen
          as a victim, and we got the lock immediately: no need to
          wait then */
  dberr_t add_to_waitq(const lock_t *wait_for, const lock_prdt_t *prdt = NULL);

  /**
  Create a lock for a transaction and initialise it.
  @param[in, out] trx		Transaction requesting the new lock
  @param[in] add_to_hash		add the lock to hash table
  @param[in] prdt			Predicate lock (optional)
  @return new lock instance */
  lock_t *create(trx_t *trx, bool add_to_hash,
                 const lock_prdt_t *prdt = nullptr);

  /**
  Check of the lock is on m_rec_id.
  @param[in] lock			Lock to compare with
  @return true if the record lock is on m_rec_id*/
  bool is_on_row(const lock_t *lock) const;

  /**
  Create the lock instance
  @param[in, out] trx	The transaction requesting the lock
  @param[in, out] index	Index on which record lock is required
  @param[in] mode		The lock mode desired
  @param[in] rec_id	The record id
  @param[in] size		Size of the lock + bitmap requested
  @return a record lock instance */
  static lock_t *lock_alloc(trx_t *trx, dict_index_t *index, ulint mode,
                            const RecID &rec_id, ulint size);

 private:
  /*
  @return the record lock size in bytes */
  size_t lock_size() const { return (m_size); }

  /**
  Do some checks and prepare for creating a new record lock */
  void prepare() const;

  /**
  Collect the transactions that will need to be rolled back asynchronously
  @param[in, out] trx	Transaction to be rolled back */
  void mark_trx_for_rollback(trx_t *trx);

  /**
  Jump the queue for the record over all low priority transactions and
  add the lock. If all current granted locks are compatible, grant the
  lock. Otherwise, mark all granted transaction for asynchronous
  rollback and add to hit list.
  @param[in, out]	lock		Lock being requested
  @param[in]	conflict_lock	First conflicting lock from the head
  @return true if the lock is granted */
  bool jump_queue(lock_t *lock, const lock_t *conflict_lock);

  /** Find position in lock queue and add the high priority transaction
  lock. Intention and GAP only locks can be granted even if there are
  waiting locks in front of the queue. To add the High priority
  transaction in a safe position we keep the following rule.

  1. If the lock can be granted, add it before the first waiting lock
  in the queue so that all currently waiting locks need to do conflict
  check before getting granted.

  2. If the lock has to wait, add it after the last granted lock or the
  last waiting high priority transaction in the queue whichever is later.
  This ensures that the transaction is granted only after doing conflict
  check with all granted transactions.
  @param[in]      lock            Lock being requested
  @param[in]      conflict_lock   First conflicting lock from the head
  @param[out]     high_priority   high priority transaction ahead in queue
  @return true if the lock can be granted */
  bool lock_add_priority(lock_t *lock, const lock_t *conflict_lock,
                         bool *high_priority);

  /** Iterate over the granted locks and prepare the hit list for
  ASYNC Rollback.

  If the transaction is waiting for some other lock then wake up
  with deadlock error.  Currently we don't mark following transactions
  for ASYNC Rollback.

  1. Read only transactions
  2. Background transactions
  3. Other High priority transactions
  @param[in]      lock            Lock being requested
  @param[in]      conflict_lock   First conflicting lock from the head */
  void make_trx_hit_list(lock_t *lock, const lock_t *conflict_lock);

  /**
  Setup the requesting transaction state for lock grant
  @param[in,out] lock	Lock for which to change state */
  void set_wait_state(lock_t *lock);

  /**
  Add the lock to the record lock hash and the transaction's lock list
  @param[in,out] lock	Newly created record lock to add to the
                          rec hash and the transaction lock list
  @param[in] add_to_hash	If the lock should be added to the hash table */
  void lock_add(lock_t *lock, bool add_to_hash);

  /**
  Check and resolve any deadlocks
  @param[in, out] lock		The lock being acquired
  @return DB_LOCK_WAIT, DB_DEADLOCK, or
          DB_SUCCESS_LOCKED_REC; DB_SUCCESS_LOCKED_REC means that
          there was a deadlock, but another transaction was chosen
          as a victim, and we got the lock immediately: no need to
          wait then */
  dberr_t deadlock_check(lock_t *lock);

  /**
  Check the outcome of the deadlock check
  @param[in,out] victim_trx	Transaction selected for rollback
  @param[in,out] lock		Lock being requested
  @return DB_LOCK_WAIT, DB_DEADLOCK or DB_SUCCESS_LOCKED_REC */
  dberr_t check_deadlock_result(const trx_t *victim_trx, lock_t *lock);

  /**
  Setup the context from the requirements */
  void init(const page_t *page) {
    ut_ad(lock_mutex_own());
    ut_ad(!srv_read_only_mode);
    ut_ad(m_index->is_clustered() || !dict_index_is_online_ddl(m_index));
    ut_ad(m_thr == NULL || m_trx == thr_get_trx(m_thr));

    m_size = is_predicate_lock(m_mode) ? lock_size(m_mode) : lock_size(page);

    /** If rec is the supremum record, then we reset the
    gap and LOCK_REC_NOT_GAP bits, as all locks on the
    supremum are automatically of the gap type */

    if (m_rec_id.m_heap_no == PAGE_HEAP_NO_SUPREMUM) {
      ut_ad(!(m_mode & LOCK_REC_NOT_GAP));

      m_mode &= ~(LOCK_GAP | LOCK_REC_NOT_GAP);
    }
  }

  /**
  Calculate the record lock physical size required for a predicate lock.
  @param[in] mode For predicate locks the lock mode
  @return the size of the lock data structure required in bytes */
  static size_t lock_size(ulint mode) {
    ut_ad(is_predicate_lock(mode));

    /* The lock is always on PAGE_HEAP_NO_INFIMUM(0),
    so we only need 1 bit (which is rounded up to 1
    byte) for lock bit setting */

    size_t n_bytes;

    if (mode & LOCK_PREDICATE) {
      const ulint align = UNIV_WORD_SIZE - 1;

      /* We will attach the predicate structure
      after lock. Make sure the memory is
      aligned on 8 bytes, the mem_heap_alloc
      will align it with MEM_SPACE_NEEDED
      anyway. */

      n_bytes = (1 + sizeof(lock_prdt_t) + align) & ~align;

      /* This should hold now */

      ut_ad(n_bytes == sizeof(lock_prdt_t) + UNIV_WORD_SIZE);

    } else {
      n_bytes = 1;
    }

    return (n_bytes);
  }

  /**
  Calculate the record lock physical size required, non-predicate lock.
  @param[in] page		For non-predicate locks the buffer page
  @return the size of the lock data structure required in bytes */
  static size_t lock_size(const page_t *page) {
    ulint n_recs = page_dir_get_n_heap(page);

    /* Make lock bitmap bigger by a safety margin */

    return (1 + ((n_recs + LOCK_PAGE_BITMAP_MARGIN) / 8));
  }

  /**
  @return true if the requested lock mode is for a predicate
          or page lock */
  static bool is_predicate_lock(ulint mode) {
    return (mode & (LOCK_PREDICATE | LOCK_PRDT_PAGE));
  }

 private:
  /** The query thread of the transaction */
  que_thr_t *m_thr;

  /**
  Transaction requesting the record lock */
  trx_t *m_trx;

  /**
  Lock mode requested */
  ulint m_mode;

  /**
  Size of the record lock in bytes */
  size_t m_size;

  /**
  Index on which the record lock is required */
  dict_index_t *m_index;

  /**
  The record lock tuple {space, page_no, heap_no} */
  RecID m_rec_id;
};

#ifdef UNIV_DEBUG
/** The count of the types of locks. */
static const ulint lock_types = UT_ARR_SIZE(lock_compatibility_matrix);
#endif /* UNIV_DEBUG */

/** Gets the type of a lock.
 @return LOCK_TABLE or LOCK_REC */
UNIV_INLINE
uint32_t lock_get_type_low(const lock_t *lock); /*!< in: lock */

/** Gets the previous record lock set on a record.
 @return previous lock on the same record, NULL if none exists */
const lock_t *lock_rec_get_prev(
    const lock_t *in_lock, /*!< in: record lock */
    ulint heap_no);        /*!< in: heap number of the record */

/** Cancels a waiting lock request and releases possible other transactions
waiting behind it.
@param[in,out]	lock		Waiting lock request
@param[in]	use_fcfs	true -> use first come first served strategy */
void lock_cancel_waiting_and_release(lock_t *lock, bool use_fcfs);

/** Checks if some transaction has an implicit x-lock on a record in a clustered
 index.
 @return transaction id of the transaction which has the x-lock, or 0 */
UNIV_INLINE
trx_id_t lock_clust_rec_some_has_impl(
    const rec_t *rec,          /*!< in: user record */
    const dict_index_t *index, /*!< in: clustered index */
    const ulint *offsets)      /*!< in: rec_get_offsets(rec, index) */
    MY_ATTRIBUTE((warn_unused_result));

/** Gets the first or next record lock on a page.
 @return next lock, NULL if none exists */
UNIV_INLINE
const lock_t *lock_rec_get_next_on_page_const(
    const lock_t *lock); /*!< in: a record lock */

/** Gets the nth bit of a record lock.
@param[in]	lock	record lock
@param[in]	i	index of the bit
@return true if bit set also if i == ULINT_UNDEFINED return false */
UNIV_INLINE
bool lock_rec_get_nth_bit(const lock_t *lock, ulint i);

/** Gets the number of bits in a record lock bitmap.
 @return number of bits */
UNIV_INLINE
ulint lock_rec_get_n_bits(const lock_t *lock); /*!< in: record lock */

/** Sets the nth bit of a record lock to TRUE.
@param[in]	lock	record lock
@param[in]	i	index of the bit */
UNIV_INLINE
void lock_rec_set_nth_bit(lock_t *lock, ulint i);

/** Gets the first or next record lock on a page.
 @return next lock, NULL if none exists */
UNIV_INLINE
lock_t *lock_rec_get_next_on_page(lock_t *lock); /*!< in: a record lock */

/** Gets the first record lock on a page, where the page is identified by its
file address.
@param[in]	lock_hash	lock hash table
@param[in]	space		space
@param[in]	page_no		page number
@return first lock, NULL if none exists */
UNIV_INLINE
lock_t *lock_rec_get_first_on_page_addr(hash_table_t *lock_hash,
                                        space_id_t space, page_no_t page_no);

/** Gets the first record lock on a page, where the page is identified by a
pointer to it.
@param[in]	lock_hash	lock hash table
@param[in]	block		buffer block
@return first lock, NULL if none exists */
UNIV_INLINE
lock_t *lock_rec_get_first_on_page(hash_table_t *lock_hash,
                                   const buf_block_t *block);

/** Gets the next explicit lock request on a record.
@param[in]	heap_no	heap number of the record
@param[in]	lock	lock
@return next lock, NULL if none exists or if heap_no == ULINT_UNDEFINED */
UNIV_INLINE
lock_t *lock_rec_get_next(ulint heap_no, lock_t *lock);

/** Gets the next explicit lock request on a record.
@param[in]	heap_no	heap number of the record
@param[in]	lock	lock
@return next lock, NULL if none exists or if heap_no == ULINT_UNDEFINED */
UNIV_INLINE
const lock_t *lock_rec_get_next_const(ulint heap_no, const lock_t *lock);

/** Gets the first explicit lock request on a record.
@param[in]	hash		Record hash
@param[in]	rec_id		Record ID
@return	first lock, nullptr if none exists */
UNIV_INLINE
lock_t *lock_rec_get_first(hash_table_t *hash, const RecID &rec_id);

/** Gets the first explicit lock request on a record.
@param[in]	hash	hash chain the lock on
@param[in]	block	block containing the record
@param[in]	heap_no	heap number of the record
@return first lock, NULL if none exists */
UNIV_INLINE
lock_t *lock_rec_get_first(hash_table_t *hash, const buf_block_t *block,
                           ulint heap_no);

/** Gets the mode of a lock.
 @return mode */
UNIV_INLINE
enum lock_mode lock_get_mode(const lock_t *lock); /*!< in: lock */

/** Calculates if lock mode 1 is compatible with lock mode 2.
@param[in]	mode1	lock mode
@param[in]	mode2	lock mode
@return nonzero if mode1 compatible with mode2 */
UNIV_INLINE
ulint lock_mode_compatible(enum lock_mode mode1, enum lock_mode mode2);

/** Calculates if lock mode 1 is stronger or equal to lock mode 2.
@param[in]	mode1	lock mode
@param[in]	mode2	lock mode
@return nonzero if mode1 stronger or equal to mode2 */
UNIV_INLINE
ulint lock_mode_stronger_or_eq(enum lock_mode mode1, enum lock_mode mode2);

/** Gets the wait flag of a lock.
 @return LOCK_WAIT if waiting, 0 if not */
UNIV_INLINE
ulint lock_get_wait(const lock_t *lock); /*!< in: lock */

/** Looks for a suitable type record lock struct by the same trx on the same
page. This can be used to save space when a new record lock should be set on a
page: no new struct is needed, if a suitable old is found.
@param[in]	type_mode	lock type_mode field
@param[in]	heap_no		heap number of the record
@param[in]	lock		lock_rec_get_first_on_page()
@param[in]	trx		transaction
@return lock or NULL */
UNIV_INLINE
lock_t *lock_rec_find_similar_on_page(ulint type_mode, ulint heap_no,
                                      lock_t *lock, const trx_t *trx);

/** Checks if a transaction has the specified table lock, or stronger. This
function should only be called by the thread that owns the transaction.
@param[in]	trx	transaction
@param[in]	table	table
@param[in]	mode	lock mode
@return lock or NULL */
UNIV_INLINE
const lock_t *lock_table_has(const trx_t *trx, const dict_table_t *table,
                             enum lock_mode mode);

#include "lock0priv.ic"

/** Iterate over record locks matching <space, page_no, heap_no> */
struct Lock_iter {
  /* First is the previous lock, and second is the current lock. */
  /** Gets the next record lock on a page.
  @param[in]	rec_id		The record ID
  @param[in]	lock		The current lock
  @return matching lock or nullptr if end of list */
  static lock_t *advance(const RecID &rec_id, lock_t *lock) {
    ut_ad(lock_mutex_own());
    ut_ad(lock->is_record_lock());

    while ((lock = static_cast<lock_t *>(lock->hash)) != nullptr) {
      ut_ad(lock->is_record_lock());

      if (rec_id.matches(lock)) {
        return (lock);
      }
    }

    ut_ad(lock == nullptr);
    return (nullptr);
  }

  /** Gets the first explicit lock request on a record.
  @param[in]	list		Record hash
  @param[in]	rec_id		Record ID
  @return	first lock, nullptr if none exists */
  static lock_t *first(hash_cell_t *list, const RecID &rec_id) {
    ut_ad(lock_mutex_own());

    auto lock = static_cast<lock_t *>(list->node);

    ut_ad(lock == nullptr || lock->is_record_lock());

    if (lock != nullptr && !rec_id.matches(lock)) {
      lock = advance(rec_id, lock);
    }

    return (lock);
  }

  /** Iterate over all the locks on a specific row
  @param[in]	rec_id		Iterate over locks on this row
  @param[in]	f		Function to call for each entry
  @return lock where the callback returned false */
  template <typename F>
  static const lock_t *for_each(const RecID &rec_id, F &&f) {
    ut_ad(lock_mutex_own());

    auto hash_table = lock_sys->rec_hash;

    auto list = hash_get_nth_cell(hash_table,
                                  hash_calc_hash(rec_id.m_fold, hash_table));

    for (auto lock = first(list, rec_id); lock != nullptr;
         lock = advance(rec_id, lock)) {
      ut_ad(lock->is_record_lock());

      if (!f(lock)) {
        return (lock);
      }
    }

    return (nullptr);
  }

  /** Iterate over locks starting from begin and up to end
  @param[in]	begin		Starting point
  @param[in]	end		Up to but not including
  @param[in]	heap_no		Heap number in the block
  @param[in]	f		Function to call for each entry
  @return lock where where the iteration ended */
  template <typename F>
  static const lock_t *for_each(const lock_t *begin, const lock_t *end,
                                uint32_t heap_no, F &&f) {
    ut_ad(lock_mutex_own());
    ut_ad(end->is_record_lock());
    ut_ad(begin->is_record_lock());

    ut_ad(begin->rec_lock.space == end->rec_lock.space);

    ut_ad(begin->rec_lock.page_no == end->rec_lock.page_no);

    for (auto lock = begin; lock != nullptr && lock != end;
         lock = lock_rec_get_next_const(heap_no, lock)) {
      ut_ad(lock->is_record_lock());

      if (!f(lock)) {
        return (lock);
      }
    }

    return (nullptr);
  }
};

#endif /* lock0priv_h */
