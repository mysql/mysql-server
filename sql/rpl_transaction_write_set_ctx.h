/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_TRANSACTION_WRITE_SET_CTX_H
#define RPL_TRANSACTION_WRITE_SET_CTX_H

#include <stddef.h>
#include <atomic>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "my_inttypes.h"

/**
  Thread class responsible for the collection of write sets associated
  to a transaction.

  It also includes support for save points where information will be discarded
  on rollbacks to a savepoint.

  Write set and flags are reset on
    Rpl_transaction_write_set_ctx::reset_state().

  The write set collection by an executing transaction is capped to a limit.
  The limit can be "soft" or "hard":
  - when a writeset grows above a "soft" limit, the transaction is allowed to
  execute and commit, but the write set is discarded, and the transaction
  declared to not have a usable write set.
  - when a write set grows above a "hard" limit, the transaction is forced
  to abort and rollback.

  We cannot use a soft limit for transactions that will be certified in GR,
  since a writeset is required for correct certification. But when GR is
  disabled, we can use a soft limit, because the writeset is only used to
  compute transaction dependencies, and we can pessimistically mark the
  transaction as conflicting with all other transcations when the writeset
  is discarded.
  A soft limit can also be used for transactions executed by the GR recovery
  channel, since they will not be certified, and the GR applier channel,
  since those transactions have already passed the certification stage.

  For the soft limit, we use
    - binlog_transaction_dependency_history_size.
  Transactions bigger than that cannot be added to the writeset history
  since they do not fit, and therefore are marked as conflicting with all
  *subsequent* transactions anyways.
  Therefore much of the parallelization for the transaction is already
  destroyed, and it is unlikely that also marking it as conflicting with
  *previous* transactions makes a significant difference.

  For the hard limit, when using Group Replication, we use
    - group_replication_transaction_size_limit.
  Since the writeset is a subset of the transaction, and the transaction
  is limited to this size anyways, a transaction whose writeset exceeds
  this limit will fail anyways.
  So failing it when generating the writeset is merely a fail-fast mechanism,
  and it is not a restriction to apply this limit to the writeset of all
  transactions for which the full transaction data is also subject to the limit.
  The transactions that are subject to this limit are exactly those executed
  when GR is enabled, except by the GR applier channel and GR recovery channel.

  We expose the following interfaces so that components such as GR
  can control the limit.
  There is an interface to *globally*
   disable/enable the soft limit:
     static void set_global_require_full_write_set(bool requires_ws);
   set/alter/remove a hard limit:
     static void set_global_write_set_memory_size_limit(uint64 limit)
     static void update_global_write_set_memory_size_limit(uint64 limit);

  There is another interface to override the global limits for a thread:
    void set_local_ignore_write_set_memory_limit(bool ignore_limit);
    void set_local_allow_drop_write_set(bool allow_drop_write_set);

  The local methods are used for example for Group Replication applier and
  recovery threads as group_replication_transaction_size_limit only applies
  to client sessions and non group replication replica threads.
*/
class Rpl_transaction_write_set_ctx {
 public:
  Rpl_transaction_write_set_ctx();
  virtual ~Rpl_transaction_write_set_ctx() = default;

  /**
    Function to add the write set of the hash of the PKE in the std::vector
    in the transaction_ctx object.

    @param[in] hash - the uint64 type hash value of the PKE.

    @return true if it can't add the write set entry, false if successful
  */
  bool add_write_set(uint64 hash);

  /*
    Function to get the pointer of the write set vector in the
    transaction_ctx object.
  */
  std::vector<uint64> *get_write_set();

  /**
    Reset the object so it can be used for a new transaction.
  */
  void reset_state();

  /*
    mark transactions that include tables with no pk
  */
  void set_has_missing_keys();

  /*
    check if the transaction was marked as having missing keys.

    @retval true  The transaction accesses tables with no PK.
    @retval false All tables referenced in transaction have PK.
   */
  bool get_has_missing_keys();

  /*
    mark transactions that include tables referenced by foreign keys
  */
  void set_has_related_foreign_keys();

  /*
    function to check if the transaction was marked as having missing keys.

    @retval true  If the transaction was marked as being referenced by a foreign
    key
  */
  bool get_has_related_foreign_keys();

  /**
    Identifies situations where the limit for number of write set entries
    already exceeded the configure limit.

    @retval true if too many write set entries exist, false otherwise
  */
  bool was_write_set_limit_reached();

  /**
    @returns the size of the write_set field in bytes
  */
  size_t write_set_memory_size();

  /**
    Function to add a new SAVEPOINT identifier in the savepoint map in the
    transaction_ctx object.

    @param[in] name - the identifier name of the SAVEPOINT.
  */
  void add_savepoint(char *name);

  /**
    Function to delete a SAVEPOINT identifier in the savepoint map in the
    transaction_ctx object.

    @param[in] name - the identifier name of the SAVEPOINT.
  */
  void del_savepoint(char *name);

  /**
    Function to delete all data added to write set and savepoint since
    SAVEPOINT identifier was added to savepoinbt in the transaction_ctx object.

    @param[in] name - the identifier name of the SAVEPOINT.
  */
  void rollback_to_savepoint(char *name);

  /**
    Function to push savepoint data to a list and clear the savepoint map in
    order to create another identifier context, needed on functions ant trigger.
  */
  void reset_savepoint_list();

  /**
    Restore previous savepoint map context, called after executed trigger or
    function.
  */
  void restore_savepoint_list();

  /**
    Adds a memory limit for write sets.

    @note currently only one component can set this limit a time.

    @param limit the limit to be added
  */
  static void set_global_write_set_memory_size_limit(uint64 limit);

  /**
    Updates the memory limit for write sets.

    @note Using the value 0 disables the limit

    @param limit the limit to be added
  */
  static void update_global_write_set_memory_size_limit(uint64 limit);

  /**
    Prevent or allow this class to discard writesets exceeding a size limit
    If true, a transaction will never discard its write sets

    @param requires_ws if who invoked the method needs or not write sets
  */
  static void set_global_require_full_write_set(bool requires_ws);

  /**
    Set if the thread shall ignore any configured memory limit
    for write set collection

    @param ignore_limit if the limit should be ignored
  */
  void set_local_ignore_write_set_memory_limit(bool ignore_limit);

  /**
    Set if the thread shall if needed discard write sets

    @param allow_drop_write_set if full write sets are not critical
  */
  void set_local_allow_drop_write_set(bool allow_drop_write_set);

 private:
  /*
    Clear the vector that stores the PKEs, and clear the savepoints, but do not
    restore all the flags. Outside transaction cleanup, this is used when
    discarding a writeset of excessive size, without aborting the transaction.
  */
  void clear_write_set();

  std::vector<uint64> write_set;
  bool m_has_missing_keys;
  bool m_has_related_foreign_keys;

  /**
    Contains information related to SAVEPOINTs. The key on map is the
    identifier and the value is the size of write set when command was
    executed.
  */
  std::map<std::string, size_t> savepoint;

  /**
    Create a savepoint context hierarchy to support encapsulation of
    identifier name when function or trigger are executed.
  */
  std::list<std::map<std::string, size_t>> savepoint_list;

  // Write set restriction variables

  /** There is a component requiring write sets on transactions */
  static std::atomic<bool> m_global_component_requires_write_sets;
  /** Memory size limit enforced for write set collection */
  static std::atomic<uint64> m_global_write_set_memory_size_limit;

  /**
    If the thread should or not ignore the set limit for
    write set collection
  */
  bool m_ignore_write_set_memory_limit;
  /**
    Even if a component says all transactions require write sets,
    this variable says this thread should discard them when they are
    bigger than m_opt_max_history_size
  */
  bool m_local_allow_drop_write_set;

  /** True if the write set size is over the configure limit */
  bool m_local_has_reached_write_set_limit;
};

#endif /* RPL_TRANSACTION_WRITE_SET_CTX_H */
