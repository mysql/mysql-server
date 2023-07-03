/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_conflict_trans.h"

#include "my_alloc.h"
#include "my_base.h"
#include "my_byteorder.h"
#include "my_sys.h"

/* Whether to track all transactions, or just
 * 'interesting' ones
 */
#define TRACK_ALL_TRANSACTIONS 0

/* Whether to check the transaction graph for
 * correctness at runtime
 */
#define CHECK_TRANS_GRAPH 0

/* st_row_event_key_info implementation */

st_row_event_key_info::st_row_event_key_info(const NdbDictionary::Table *_table,
                                             const uchar *_key_buff,
                                             Uint32 _key_buff_len,
                                             Uint64 _transaction_id)
    : tableObj(_table),
      packed_key(_key_buff),
      packed_key_len(_key_buff_len),
      transaction_id(_transaction_id),
      hash_next(nullptr) {}

Uint64 st_row_event_key_info::getTransactionId() const {
  return transaction_id;
}

void st_row_event_key_info::updateRowTransactionId(Uint64 mostRecentTransId) {
  transaction_id = mostRecentTransId;
}

Uint32 st_row_event_key_info::hashValue() const {
  /* Include Table Object Id + primary key */
  Uint32 h = (17 * 37) + tableObj->getObjectId();
  for (Uint32 i = 0; i < packed_key_len; i++) h = (37 * h) + packed_key[i];
  return h;
}

bool st_row_event_key_info::equal(const st_row_event_key_info *other) const {
  /* Check same table + same PK */
  return ((tableObj == other->tableObj) &&
          (packed_key_len == other->packed_key_len) &&
          (memcmp(packed_key, other->packed_key, packed_key_len) == 0));
}

st_row_event_key_info *st_row_event_key_info::getNext() const {
  return hash_next;
}

void st_row_event_key_info::setNext(st_row_event_key_info *_next) {
  hash_next = _next;
}

/* st_trans_dependency implementation */

st_trans_dependency::st_trans_dependency(st_transaction *_target_transaction,
                                         st_transaction *_dependent_transaction,
                                         const st_trans_dependency *_next)
    : target_transaction(_target_transaction),
      dependent_transaction(_dependent_transaction),
      next_entry(_next),
      hash_next(nullptr) {}

st_transaction *st_trans_dependency::getTargetTransaction() const {
  return target_transaction;
}

st_transaction *st_trans_dependency::getDependentTransaction() const {
  return dependent_transaction;
}

const st_trans_dependency *st_trans_dependency::getNextDependency() const {
  return next_entry;
}

Uint32 st_trans_dependency::hashValue() const {
  /* Hash the ptrs in a rather nasty way */
  UintPtr p = ((UintPtr)target_transaction) ^ ((UintPtr)dependent_transaction);
  ;

  if (sizeof(p) == 8) {
    /* Xor two words of 64 bit ptr */
    p = (p & 0xffffffff) ^ ((((Uint64)p) >> 32) & 0xffffffff);
  }

  return 17 + (37 * (Uint32)p);
}

bool st_trans_dependency::equal(const st_trans_dependency *other) const {
  return ((target_transaction == other->target_transaction) &&
          (dependent_transaction == other->dependent_transaction));
}

st_trans_dependency *st_trans_dependency::getNext() const { return hash_next; }

void st_trans_dependency::setNext(st_trans_dependency *_next) {
  hash_next = _next;
}

/* st_transaction implementation */

st_transaction::st_transaction(Uint64 _transaction_id)
    : transaction_id(_transaction_id),
      in_conflict(false),
      dependency_list_head(nullptr),
      hash_next(nullptr) {}

Uint64 st_transaction::getTransactionId() const { return transaction_id; }

bool st_transaction::getInConflict() const { return in_conflict; }

void st_transaction::setInConflict() { in_conflict = true; }

const st_trans_dependency *st_transaction::getDependencyListHead() const {
  return dependency_list_head;
}

void st_transaction::setDependencyListHead(st_trans_dependency *head) {
  dependency_list_head = head;
}

/* Hash Api */
Uint32 st_transaction::hashValue() const {
  return 17 + (37 * ((transaction_id & 0xffffffff) ^
                     (transaction_id >> 32 & 0xffffffff)));
}

bool st_transaction::equal(const st_transaction *other) const {
  return transaction_id == other->transaction_id;
}

st_transaction *st_transaction::getNext() const { return hash_next; }

void st_transaction::setNext(st_transaction *_next) { hash_next = _next; }

/*
   Unique HashMap(Set) of st_row_event_key_info ptrs, with bucket storage
   allocated from st_mem_root_allocator
*/
template class HashMap2<st_row_event_key_info, true, st_mem_root_allocator>;
template class HashMap2<st_transaction, true, st_mem_root_allocator>;
template class HashMap2<st_trans_dependency, true, st_mem_root_allocator>;
template class LinkedStack<Uint64, st_mem_root_allocator>;

/**
 * pack_key_to_buffer
 *
 * Given a table, key_record and record, this method will
 * determine how many significant bytes the key contains,
 * and if a buffer is passed, will copy the bytes into the
 * buffer. Returns -1 on failure and 0 on success
 */
static int pack_key_to_buffer(const NdbDictionary::Table *table,
                              const NdbRecord *key_rec, const uchar *record,
                              uchar *buffer, Uint32 &buff_len) {
  /* Loop over attributes in key record, determining their actual
   * size based on column type and contents of record
   * If buffer supplied, copy them contiguously to buffer
   * return total length
   */
  Uint32 attr_id;
  Uint32 buff_offset = 0;
  if (!NdbDictionary::getFirstAttrId(key_rec, attr_id)) return -1;

  do {
    Uint32 from_offset = 0;
    Uint32 byte_len = 0;
    const NdbDictionary::Column *key_col = table->getColumn(attr_id);
    NdbDictionary::getOffset(key_rec, attr_id, from_offset);
    assert(!NdbDictionary::isNull(key_rec, (const char *)record, attr_id));

    switch (key_col->getArrayType()) {
      case NDB_ARRAYTYPE_FIXED:
        byte_len = key_col->getSizeInBytes();
        break;
      case NDB_ARRAYTYPE_SHORT_VAR:
        byte_len = record[from_offset];
        from_offset++;
        break;
      case NDB_ARRAYTYPE_MEDIUM_VAR:
        byte_len = uint2korr(&record[from_offset]);
        from_offset += 2;
        break;
      default:
        assert(false);
        return -1;
    };
    assert((buff_offset + byte_len) <= buff_len);

    if (buffer) memcpy(&buffer[buff_offset], &record[from_offset], byte_len);

    buff_offset += byte_len;
  } while (NdbDictionary::getNextAttrId(key_rec, attr_id));

  buff_len = buff_offset;
  return 0;
}

static bool determine_packed_key_size(const NdbDictionary::Table *table,
                                      const NdbRecord *key_rec,
                                      const uchar *record,
                                      Uint32 &required_buff_size) {
  /* Use pack_key_to_buffer to calculate length required */
  if (pack_key_to_buffer(table, key_rec, record, nullptr, required_buff_size) ==
      -1)
    return false;
  return true;
}

/* st_mem_root_allocator implementation */
void *st_mem_root_allocator::alloc(void *ctx, size_t bytes) {
  st_mem_root_allocator *a = (st_mem_root_allocator *)ctx;
  return a->mem_root->Alloc(bytes);
}

void *st_mem_root_allocator::mem_calloc(void *ctx, size_t nelem, size_t bytes) {
  st_mem_root_allocator *a = (st_mem_root_allocator *)ctx;
  return a->mem_root->Alloc(nelem * bytes);
}

void st_mem_root_allocator::mem_free(void *, void *) {
  /* Do nothing, will be globally freed when arena (mem_root)
   * released
   */
}

st_mem_root_allocator::st_mem_root_allocator(MEM_ROOT *_mem_root)
    : mem_root(_mem_root) {}

/* DependencyTracker implementation */

DependencyTracker *DependencyTracker::newDependencyTracker(MEM_ROOT *mem_root) {
  DependencyTracker *dt = nullptr;
  // Allocate memory from MEM_ROOT
  void *mem = mem_root->Alloc(sizeof(DependencyTracker));
  if (mem) {
    dt = new (mem) DependencyTracker(mem_root);
  }

  return dt;
}

DependencyTracker::DependencyTracker(MEM_ROOT *mem_root)
    : mra(mem_root),
      key_hash(&mra),
      trans_hash(&mra),
      dependency_hash(&mra),
      iteratorTodo(ITERATOR_STACK_BLOCKSIZE, &mra),
      conflicting_trans_count(0),
      error_text(nullptr) {
  /* TODO Get sizes from somewhere */
  key_hash.setSize(1024);
  trans_hash.setSize(100);
  dependency_hash.setSize(100);
}

int DependencyTracker::track_operation(const NdbDictionary::Table *table,
                                       const NdbRecord *key_rec,
                                       const uchar *row,
                                       Uint64 transaction_id) {
  DBUG_TRACE;

  Uint32 required_buff_size = ~Uint32(0);
  if (!determine_packed_key_size(table, key_rec, row, required_buff_size)) {
    if (!error_text)
      error_text = "track_operation : Failed to determine packed key size";
    return -1;
  }
  DBUG_PRINT("info", ("Required length for key : %u", required_buff_size));

  /* Alloc space for packed key and struct in MEM_ROOT */
  uchar *packed_key_buff = (uchar *)mra.mem_root->Alloc(required_buff_size);
  void *element_mem = mra.mem_root->Alloc(sizeof(st_row_event_key_info));

  if (pack_key_to_buffer(table, key_rec, row, packed_key_buff,
                         required_buff_size) == -1) {
    if (!error_text) error_text = "track_operation : Failed packing key";
    return -1;
  }

  if (TRACK_ALL_TRANSACTIONS) {
    st_transaction *transEntry = get_or_create_transaction(transaction_id);
    if (!transEntry) {
      error_text = "track_operation : Failed to get or create transaction";
      return HA_ERR_OUT_OF_MEM;
    }
  }

  st_row_event_key_info *key_info = new (element_mem) st_row_event_key_info(
      table, packed_key_buff, required_buff_size, transaction_id);

  /* Now try to add element to hash */
  if (!key_hash.add(key_info)) {
    /*
      Already an element in the keyhash with this primary key
      If it's for the same transaction then ignore, otherwise
      it's an inter-transaction dependency
    */
    st_row_event_key_info *existing = key_hash.get(key_info);

    Uint64 existingTransIdOnRow = existing->getTransactionId();
    Uint64 newTransIdOnRow = key_info->getTransactionId();

    if (existingTransIdOnRow != newTransIdOnRow) {
      int res = add_dependency(existingTransIdOnRow, newTransIdOnRow);
      /*
        Update stored transaction_id to be latest for key.
        Further key operations on this row will depend on this
        transaction, and transitively on the previous
        transaction.
      */
      existing->updateRowTransactionId(newTransIdOnRow);

      assert(res == 0 || error_text != nullptr);

      return res;
    } else {
      /*
         How can we have two updates to the same row with the
         same transaction id?  Only if the transaction id
         is invalid (e.g. not set)
         In normal cases with only one upstream master, each
         distinct master user transaction will have a unique
         id, and all operations on a row in that transaction
         will be merged in TUP prior to emitting a SUMA
         event.
         This could be relaxed for more complex upstream
         topologies, but acts as a sanity guard currently.
      */
      if (existingTransIdOnRow != InvalidTransactionId) {
        assert(false);
        error_text =
            "Two row operations to same key sharing user transaction id";
        return -1;
      }
    }
  }

  return 0;
}

int DependencyTracker::mark_conflict(Uint64 trans_id) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("trans_id : %llu", trans_id));

  st_transaction *entry = get_or_create_transaction(trans_id);
  if (!entry) {
    error_text = "mark_conflict : get_or_create_transaction() failure";
    return HA_ERR_OUT_OF_MEM;
  }

  if (entry->getInConflict()) {
    /* Nothing to do here */
    return 0;
  }

  /* Have entry, mark it, and any dependents */
  bool fetch_node_dependents;
  st_transaction *dependent = entry;
  reset_dependency_iterator();
  do {
    DBUG_PRINT("info",
               ("Visiting transaction %llu, conflict : %u",
                dependent->getTransactionId(), dependent->getInConflict()));
    /*
      If marked already, don't fetch dependents, as
      they will also be marked already
    */
    fetch_node_dependents = false;

    if (!dependent->getInConflict()) {
      dependent->setInConflict();
      conflicting_trans_count++;
      fetch_node_dependents = true;
    }
  } while ((dependent = get_next_dependency(dependent, fetch_node_dependents)));

  assert(verify_graph());

  return 0;
}

bool DependencyTracker::in_conflict(Uint64 trans_id) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("trans_id %llu", trans_id));
  st_transaction key(trans_id);
  const st_transaction *entry = nullptr;

  /*
    If transaction hash entry exists, check it for
    conflicts.  If it doesn't exist, no conflict
  */
  if ((entry = trans_hash.get(&key))) {
    DBUG_PRINT("info", ("in_conflict : %u", entry->getInConflict()));
    return entry->getInConflict();
  } else {
    assert(!TRACK_ALL_TRANSACTIONS);
  }
  return false;
}

st_transaction *DependencyTracker::get_or_create_transaction(Uint64 trans_id) {
  DBUG_TRACE;
  st_transaction transKey(trans_id);
  st_transaction *transEntry = nullptr;

  if (!(transEntry = trans_hash.get(&transKey))) {
    /*
       Transaction does not exist.  Allocate it
       and add to the hash
    */
    DBUG_PRINT("info",
               ("Creating new hash entry for transaction (%llu)", trans_id));

    transEntry = (st_transaction *)st_mem_root_allocator::alloc(
        &mra, sizeof(st_transaction));

    if (transEntry) {
      new (transEntry) st_transaction(trans_id);

      if (!trans_hash.add(transEntry)) {
        st_mem_root_allocator::mem_free(&mra, transEntry); /* For show */
        transEntry = nullptr;
      }
    }
  }

  return transEntry;
}

int DependencyTracker::add_dependency(Uint64 trans_id,
                                      Uint64 dependent_trans_id) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("Recording dependency of %llu on %llu",
                      dependent_trans_id, trans_id));
  st_transaction *targetEntry = get_or_create_transaction(trans_id);
  if (!targetEntry) {
    error_text = "add_dependency : Failed get_or_create_transaction";
    return HA_ERR_OUT_OF_MEM;
  }

  st_transaction *dependentEntry =
      get_or_create_transaction(dependent_trans_id);
  if (!dependentEntry) {
    error_text = "add_dependency : Failed get_or_create_transaction";
    return HA_ERR_OUT_OF_MEM;
  }

  /* Now lookup dependency.  Add it if not already present */
  st_trans_dependency depKey(targetEntry, dependentEntry, nullptr);
  st_trans_dependency *dep = dependency_hash.get(&depKey);
  if (dep == nullptr) {
    DBUG_PRINT("info", ("Creating new dependency hash entry for "
                        "dependency of %llu on %llu.",
                        dependentEntry->getTransactionId(),
                        targetEntry->getTransactionId()));

    dep = (st_trans_dependency *)st_mem_root_allocator::alloc(
        &mra, sizeof(st_trans_dependency));

    new (dep) st_trans_dependency(targetEntry, dependentEntry,
                                  targetEntry->getDependencyListHead());

    targetEntry->setDependencyListHead(dep);

    /* New dependency, propagate in_conflict if necessary */
    if (targetEntry->getInConflict()) {
      DBUG_PRINT("info", ("Marking new dependent as in-conflict"));
      return mark_conflict(dependentEntry->getTransactionId());
    }
  }

  assert(verify_graph());

  return 0;
}

void DependencyTracker::reset_dependency_iterator() { iteratorTodo.reset(); }

st_transaction *DependencyTracker::get_next_dependency(
    const st_transaction *current, bool include_dependents_of_current) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("node : %llu", current->getTransactionId()));
  /*
    Depth first traverse, with option to ignore sub graphs.
  */
  if (include_dependents_of_current) {
    /* Add dependents to stack */
    const st_trans_dependency *dependency = current->getDependencyListHead();

    while (dependency) {
      assert(dependency->getTargetTransaction() == current);
      DBUG_PRINT("info",
                 ("Adding dependency %llu->%llu",
                  dependency->getDependentTransaction()->getTransactionId(),
                  dependency->getTargetTransaction()->getTransactionId()));

      Uint64 dependentTransactionId =
          dependency->getDependentTransaction()->getTransactionId();
      iteratorTodo.push(dependentTransactionId);
      dependency = dependency->getNextDependency();
    }
  }

  Uint64 nextId;
  if (iteratorTodo.pop(nextId)) {
    DBUG_PRINT("info", ("Returning transaction id %llu", nextId));
    st_transaction key(nextId);
    st_transaction *dependent = trans_hash.get(&key);
    assert(dependent);
    return dependent;
  }

  assert(iteratorTodo.size() == 0);
  DBUG_PRINT("info", ("No more dependencies to visit"));
  return nullptr;
}

#ifndef NDEBUG
void DependencyTracker::dump_dependents(Uint64 trans_id) {
  fprintf(stderr, "Dumping dependents of transid %llu : ", trans_id);

  st_transaction key(trans_id);
  const st_transaction *dependent = nullptr;

  if ((dependent = trans_hash.get(&key))) {
    reset_dependency_iterator();
    const char *comma = ", ";
    const char *sep = "";
    do {
      {
        fprintf(stderr, "%s%llu%s", sep, dependent->getTransactionId(),
                (dependent->getInConflict() ? "-C" : ""));
        sep = comma;
      }
    } while ((dependent = get_next_dependency(dependent)));
    fprintf(stderr, "\n");
  } else {
    fprintf(stderr, "None\n");
  }
}

bool DependencyTracker::verify_graph() {
  if (!CHECK_TRANS_GRAPH) return true;

  /*
     Check the graph structure obeys its invariants

     1) There are no cycles in the graph such that
        a transaction is a dependent of itself

     2) If a transaction is marked in_conflict, all
        of its dependents (transitively), are also
        marked in conflict

     This is expensive to verify, so not always on
  */
  HashMap2<st_transaction, true, st_mem_root_allocator>::Iterator it(
      trans_hash);

  st_transaction *root = nullptr;

  while ((root = it.next())) {
    bool in_conflict = root->getInConflict();

    /* Now visit all dependents */
    st_transaction *dependent = root;
    reset_dependency_iterator();

    while ((dependent = get_next_dependency(dependent, true))) {
      if (dependent == root) {
        /* Must exit, or we'll be here forever */
        fprintf(stderr, "Error : Cycle discovered in graph\n");
        abort();
        return false;
      }

      if (in_conflict && !dependent->getInConflict()) {
        fprintf(stderr,
                "Error : Dependent transaction not marked in-conflict\n");
        abort();
        return false;
      }
    }
  }

  return true;
}
#endif

const char *DependencyTracker::get_error_text() const { return error_text; }

Uint32 DependencyTracker::get_conflict_count() const {
  return conflicting_trans_count;
}
