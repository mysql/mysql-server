/*
   Copyright (c) 2022, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XA_TRANSACTION_CACHE_H_INCLUDED
#define XA_TRANSACTION_CACHE_H_INCLUDED

#include <sys/types.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "lex_string.h"
#include "map_helpers.h"  // malloc_unordered_map
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "sql/malloc_allocator.h"  // Malloc_allocator
#include "sql/psi_memory_key.h"    // key_memory_xa_recovered_transactions
#include "sql/xa.h"                // XID
#include "sql/xa_aux.h"            // serialize_xid

class Transaction_ctx;

namespace xa {

/**
  @class Transaction_cache

  Class responsible for managing a cache of `Transaction_ctx` objects
  associated with XA transactions.

  The cache is used during the recovery stage of an XA transaction.

  @note this class is a singleton class.
 */
class Transaction_cache {
 public:
  using transaction_ptr = std::shared_ptr<Transaction_ctx>;
  using filter_predicate_t = std::function<bool(transaction_ptr const &)>;

  virtual ~Transaction_cache() = default;

  // Disallow copy/move semantics
  Transaction_cache(Transaction_cache const &) = delete;
  Transaction_cache(Transaction_cache &&) = delete;
  Transaction_cache &operator=(Transaction_cache const &) = delete;
  Transaction_cache &operator=(Transaction_cache &&) = delete;

  /**
    Transaction is marked in the cache as if it's recovered.
    The method allows to sustain prepared transaction disconnection.

    @param transaction
                   Pointer to Transaction object that is replaced.

    @return  operation result
      @retval  false   success or a cache already contains XID_STATE
                       for this XID value
      @retval  true    failure
  */
  static bool detach(Transaction_ctx *transaction);
  /**
    Remove information about transaction from a cache.

    @param transaction     Pointer to a Transaction_ctx that has to be removed
                           from a cache.
  */
  static void remove(Transaction_ctx *transaction);
  /// Mark a transaction visible through SQL XA RECOVER without changing its SQL
  /// XA state.
  ///
  /// This is used by XA PREPARE before the prepare GTID is made visible, and
  /// again when SQL state changes to XA_PREPARED. The second call is
  /// idempotent and preserves the ordinary prepared-implies-recover-visible
  /// contract for paths without GTID externalization.
  ///
  /// @param transaction Transaction context to show in SQL XA RECOVER.
  static void mark_prepared_visible_for_xa_recover(
      Transaction_ctx *transaction);
  /// Mark a transaction finalized for SQL XA RECOVER without releasing its XID
  /// reservation from the cache.
  ///
  /// This is used after XA COMMIT/XA ROLLBACK has completed in engines, but
  /// before the statement GTID is made visible. The cache entry must remain
  /// present for two reasons:
  /// 1. To keep the XID reserved until per-XID cleanup, such as MDL context
  ///    backup deletion, is done.
  /// 2. To keep the cached detached transaction identity available for
  ///    concurrent same-XID second-phase checks until final cleanup removes the
  ///    entry.
  ///
  /// @param transaction Transaction context to hide from SQL XA RECOVER.
  static void mark_finalized_for_recover(Transaction_ctx *transaction);
  /**
    Inserts a transaction context identified by a given XID.

    @param xid The XID of the transaction.
    @param transaction The object containing the context of the transaction.

    @return false if the pair was successfully inserted, true otherwise.
   */
  static bool insert(XID *xid, Transaction_ctx *transaction);
  /**
    Creates a new transaction context for the recovering transaction
    identified by a given XID.

    @param xid The XID of the transaction being recovered.

    @return false if the pair was successfully inserted, true otherwise.
   */
  static bool insert(XID *xid);
  /**
    Searches the cache for the transaction context identified by the given
    XID.

    An additional filtering predicate can be provided, to allow for further
    validations on values for mathching XID. The predicate is evaluated
    while holding the necessary locks to ensure the validaty of the
    `Transaction_ctx` shared pointer.

    A non-null value is returned if and only if:

    1. The value is found in the underlying map
    2. The found value underlying XID
       (`Transaction_ctx::xid_state()::get_xid()`) equals to the parameter
       `xid`. This validation is necessary since the XID representation for
       the key used in the underlying map isn't an exact match for the full
       XID representation.
    3. If a predicate parameter is provided, the evaluation of passing the
       value as a predicate parameter must be `true`.

    @param xid The XID of the transaction to search the context for.
    @param filter A predicate to be evaluated when an value for `xid` is
                  found. If predicate returns false, the found element is
                  filtered out.

    @return The transaction context if found and valid, nullptr otherwise.
   */
  static transaction_ptr find(XID *xid, filter_predicate_t filter = nullptr);
  /// Retrieves XID snapshots visible through SQL XA RECOVER.
  ///
  /// Entries finalized for recover are skipped here, but remain in the cache
  /// as XID reservations until final cleanup removes them.
  ///
  /// @return XID value snapshots for transactions visible through SQL
  ///         XA RECOVER.
  static std::vector<XID> get_xids_visible_to_xa_recover();
  /**
    Initializes the transaction cache underlying resources.
   */
  static void initialize();
  /**
    Disposes of the transaction cache allocated resources.
   */
  static void dispose();

 private:
  enum class Xa_recover_visibility { Visible, Hidden };

  /// Cached transaction context and its visibility through SQL XA RECOVER.
  class Cache_entry {
   public:
    /// @param transaction Transaction context stored by the cache entry.
    /// @param visibility Whether the transaction is visible through SQL
    ///                   XA RECOVER.
    Cache_entry(transaction_ptr transaction, Xa_recover_visibility visibility)
        : m_transaction(std::move(transaction)),
          m_xa_recover_visibility(visibility) {}

    /// @return Transaction context stored by the cache entry.
    const transaction_ptr &transaction() const { return m_transaction; }

    /// @retval true The transaction can be listed by SQL XA RECOVER.
    /// @retval false The transaction XID remains reserved, but hidden from SQL
    ///                XA RECOVER.
    bool is_visible_to_xa_recover() const {
      return m_xa_recover_visibility == Xa_recover_visibility::Visible;
    }

    /// Mark visible through SQL XA RECOVER without changing SQL XA state.
    void mark_prepared_visible_for_xa_recover() {
      m_xa_recover_visibility = Xa_recover_visibility::Visible;
    }

    /// Mark finalized for SQL XA RECOVER while keeping the XID reserved.
    void mark_finalized_for_recover() {
      m_xa_recover_visibility = Xa_recover_visibility::Hidden;
    }

   private:
    transaction_ptr m_transaction;
    Xa_recover_visibility m_xa_recover_visibility;
  };

  using unordered_map = malloc_unordered_map<std::string, Cache_entry>;

  /** A lock to serialize the access to `m_transaction_cache` */
  mysql_mutex_t m_LOCK_transaction_cache;
#ifdef HAVE_PSI_INTERFACE
  /** The PSI key for the above lock */
  PSI_mutex_key m_key_LOCK_transaction_cache;
  /** The PSI configuration of the above lock and key */
  PSI_mutex_info m_transaction_cache_mutexes[1] = {
      {&m_key_LOCK_transaction_cache, "LOCK_transaction_cache",
       PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}};
#endif
  /** A map holding the cached transaction context, indexed by XID */
  unordered_map m_transaction_cache;

  /**
   Class constructor.

   It's declared private since this class is a singleton class.
   */
  Transaction_cache();

  /**
    Initialize a cache to store Transaction_ctx and a mutex to protect access
    to the cache

    @return The initialized class instance.
  */
  static Transaction_cache &instance();
  /**
    Creates a new transaction context for the transaction with the given
    XID and adds it to the cache.

    @param xid The XID of the transaction to create and add.
    @param is_binlogged_arg Whether or not the transaction has already been
                            binlogged.
    @param src The transaction context and info to be added to the newly
               created cache item.

    @return false if the pair was successfully inserted, true otherwise.
   */
  static bool create_and_insert_new_transaction(XID *xid, bool is_binlogged_arg,
                                                const Transaction_ctx *src);
};
}  // namespace xa
#endif  // XA_TRANSACTION_CACHE_H_INCLUDED
