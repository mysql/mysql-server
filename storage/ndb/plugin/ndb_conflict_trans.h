/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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

#ifndef NDB_CONFLICT_TRANS_H
#define NDB_CONFLICT_TRANS_H

#include "storage/ndb/include/ndbapi/NdbApi.hpp"
#include "storage/ndb/include/util/HashMap2.hpp"
#include "storage/ndb/include/util/LinkedStack.hpp"

/*
   This file defines structures for detecting dependencies between
   transactions based on the rows they update.
   It is used when applying row updates as part of the MySQLD slave.
*/

/**
 * st_row_event_key_info
 *
 * This struct describes a row event applied by the Slave, based
 * on its table, key and transaction id.
 * Instances of this struct are placed in a hash structure where
 * the {table, key} are the key, and the transaction id is the
 * 'data'.
 * This hash is used to detect when different transactions in an
 * epoch affect the same row, which implies a dependency between
 * the transactions.
 */
class st_row_event_key_info {
 public:
  /**
   * User api
   */
  st_row_event_key_info(const NdbDictionary::Table *_table,
                        const uchar *_key_buff, Uint32 _key_buff_len,
                        Uint64 _transaction_id);
  Uint64 getTransactionId() const;
  void updateRowTransactionId(Uint64 mostRecentTransId);

  /**
   * Hash Api
   */
  Uint32 hashValue() const;
  bool equal(const st_row_event_key_info *other) const;
  st_row_event_key_info *getNext() const;
  void setNext(st_row_event_key_info *_next);

 private:
  /* Key : Table and Primary Key */
  const NdbDictionary::Table *tableObj;
  const uchar *packed_key;
  Uint32 packed_key_len;

  /* Data : Transaction id */
  Uint64 transaction_id;

  /* Next ptr for hash */
  st_row_event_key_info *hash_next;
};

class st_transaction;

/**
   st_trans_dependency
   Entry in dependency hash.
   Describes inter-transaction dependency, and comprises part
   of list of other dependents of target_transaction
*/
class st_trans_dependency {
 public:
  /* User Api */
  st_trans_dependency(st_transaction *_target_transaction,
                      st_transaction *_dependent_transaction,
                      const st_trans_dependency *_next);

  st_transaction *getTargetTransaction() const;
  st_transaction *getDependentTransaction() const;
  const st_trans_dependency *getNextDependency() const;

  /* Hash Api */
  Uint32 hashValue() const;
  bool equal(const st_trans_dependency *other) const;
  st_trans_dependency *getNext() const;
  void setNext(st_trans_dependency *_next);

 private:
  /* Key */
  st_transaction *target_transaction;
  st_transaction *dependent_transaction;

  /* Rest of co-dependents of target_transaction */
  const st_trans_dependency *next_entry;

  st_trans_dependency *hash_next;
};

/**
   st_transaction
   Entry in transaction hash, indicates whether transaction
   is in conflict, and has list of dependents
*/
class st_transaction {
 public:
  /* User Api */
  st_transaction(Uint64 _transaction_id);

  Uint64 getTransactionId() const;
  bool getInConflict() const;
  void setInConflict();
  const st_trans_dependency *getDependencyListHead() const;
  void setDependencyListHead(st_trans_dependency *head);

  /* Hash Api */
  Uint32 hashValue() const;
  bool equal(const st_transaction *other) const;
  st_transaction *getNext() const;
  void setNext(st_transaction *_next);

 private:
  /* Key */
  Uint64 transaction_id;

  /* Data */
  /* Is this transaction (and therefore its dependents) in conflict? */
  bool in_conflict;
  /* Head of list of dependencies */
  st_trans_dependency *dependency_list_head;

  /* Hash ptr */
  st_transaction *hash_next;
};

struct MEM_ROOT;

/**
 * Allocator type which internally uses a MySQLD mem_root
 * Used as a template parameter for Ndb ADTs
 */
struct st_mem_root_allocator {
  MEM_ROOT *mem_root;

  static void *alloc(void *ctx, size_t bytes);
  static void *mem_calloc(void *ctx, size_t nelem, size_t bytes);
  static void mem_free(void *ctx, void *mem);
  st_mem_root_allocator(MEM_ROOT *_mem_root);
};

class DependencyTracker {
 public:
  static const Uint64 InvalidTransactionId = ~Uint64(0);

  /**
     newDependencyTracker

     Factory method to get a DependencyTracker object, using
     memory from the passed mem_root.
     To discard dependency tracker, just free the passed mem_root.
  */
  static DependencyTracker *newDependencyTracker(MEM_ROOT *mem_root);

  /**
     track_operation

     This method records the operation on the passed
     table + primary key as belonging to the passed
     transaction.

     If there is already a recorded operation on the
     passed table + primary key from a different transaction
     then a transaction dependency is recorded.
  */
  int track_operation(const NdbDictionary::Table *table,
                      const NdbRecord *key_rec, const uchar *row,
                      Uint64 transaction_id);

  /**
     mark_conflict

     Record that a particular transaction is in conflict.
     This will also mark any dependent transactions as in
     conflict.
  */
  int mark_conflict(Uint64 trans_id);

  /**
     in_conflict

     Returns true if the supplied transaction_id is marked as
     in conflict
  */
  bool in_conflict(Uint64 trans_id);

  /**
     get_error_text

     Returns string containing error description.
     NULL if no error.
  */
  const char *get_error_text() const;

  /**
     get_conflict_count

     Returns number of transactions marked as in-conflict
  */
  Uint32 get_conflict_count() const;

 private:
  DependencyTracker(MEM_ROOT *mem_root);

  /**
     get_or_create_transaction

     Get or create the transaction object for the
     given transaction id.
     Returns Null on allocation failure.
  */
  st_transaction *get_or_create_transaction(Uint64 trans_id);

  /**
     add_dependency

     This method records a dependency between the two
     passed transaction ids
  */
  int add_dependency(Uint64 trans_id, Uint64 dependent_trans_id);

  /**
     reset_dependency_iterator

     Reset dependency iterator.
     Required before using get_next_dependency()
  */
  void reset_dependency_iterator();

  /**
     get_next_dependency
     Gets next dependency in dependency graph.
     Performs breadth first search from start node.

     include_dependents_of_current = false causes the traversal to skip
     dependents of the current node.
  */
  st_transaction *get_next_dependency(
      const st_transaction *current, bool include_dependents_of_current = true);

  /**
     dump_dependents

     Debugging function
  */
  void dump_dependents(Uint64 trans_id);

  /**
     verify_graph

     Internal invariant checking function.
  */
  bool verify_graph();

  /* MemRoot allocator class instance */
  st_mem_root_allocator mra;

  /*
     key_hash
     Map of {Table, PK} -> TransID
     Used to find inter-transaction dependencies
     Attempt to add duplicate entry to the key_hash indicates
     transaction dependency from existing entry to duplicate.
  */
  HashMap2<st_row_event_key_info, true, st_mem_root_allocator> key_hash;

  /*
     trans_hash
     Map of {TransId} -> {in_conflict, List of dependents}
     Used to record which transactions are in-conflict, and what
     their dependencies are.
     Transactions not marked in-conflict, and with no dependencies or
     dependents, are not placed in this hash.
   */
  HashMap2<st_transaction, true, st_mem_root_allocator> trans_hash;

  /*
     dependency_hash
     Map of {TransIdFrom, TransIdTo}
     Used to ensure dependencies are added only once, for efficiency.
     Elements are linked from the trans_hash entry for TransIdFrom.
   */
  HashMap2<st_trans_dependency, true, st_mem_root_allocator> dependency_hash;

  /*
     iteratorTodo
     Stack of transaction Ids to be visited during breadth first search
     when marking dependents as in conflict.
  */
  static const Uint32 ITERATOR_STACK_BLOCKSIZE = 10;
  LinkedStack<Uint64, st_mem_root_allocator> iteratorTodo;

  Uint32 conflicting_trans_count;

  const char *error_text;
};

#endif
