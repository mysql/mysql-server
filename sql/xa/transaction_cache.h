/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef XA_TRANSACTION_CACHE_H_INCLUDED
#define XA_TRANSACTION_CACHE_H_INCLUDED

#include <string.h>
#include <sys/types.h>
#include <list>
#include <mutex>

#include "lex_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "sql/malloc_allocator.h"  // Malloc_allocator
#include "sql/psi_memory_key.h"    // key_memory_xa_recovered_transactions
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
  using unordered_map = malloc_unordered_map<std::string, transaction_ptr>;
  using list = std::vector<transaction_ptr>;
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
  /**
    Retrieves the list of transaction contexts cached.

    @return A vector with all transaction contexts cached so far.
   */
  static list get_cached_transactions();
  /**
    Initializes the transaction cache underlying resources.
   */
  static void initialize();
  /**
    Disposes of the transaction cache allocated resources.
   */
  static void dispose();

 private:
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
