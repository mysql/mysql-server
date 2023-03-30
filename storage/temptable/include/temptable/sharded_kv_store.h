/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/sharded_kv_store.h
TempTable sharded key-value store implementation. */

#ifndef TEMPTABLE_SHARDED_KV_STORE_H
#define TEMPTABLE_SHARDED_KV_STORE_H

#include <array>

#include "storage/temptable/include/temptable/constants.h"
#include "storage/temptable/include/temptable/kv_store.h"
#include "storage/temptable/include/temptable/sharded_kv_store_logger.h"

namespace temptable {

/** Sharded key-value store, a convenience wrapper class around Key_value_store
 * that creates N_SHARDS instances of Key_value_store and exposes a simple
 * interface to further manipulate with them. User code, if it wishes so, can
 * opt-in for different key-value implementation or different locking
 * mechanisms. Defaults are provided as convenience.
 *
 * Mapping between a shard and an actual Key_value_store is done at the level of
 * implementation detail of this class and it is done via modulo-arithmethic on
 * THD (connection) identifier. It is guaranteed that the same shard (same
 * Key_value_store instance) will always be returned given the same input: THD
 * (connection) identifier. However, there is no guarantee that there will be
 * only one such mapping (this function is not a bijection).
 *
 * In other words, due to the wrap-around property of modulo-arithmetic
 * and depending on the actual value of N_SHARDS, it is very much possible to
 * get a reference to the same Key_value_store instance for two different THD
 * (connection) identifiers. Thread-safety guarantees are exercised at the
 * level of Key_value_store instance but are controlled through the `Lock`, this
 * class template parameter.
 *
 * Due to aforementioned modulo-arithmethic, and the questionable performance in
 * its more general form, N_SHARDS must be a number which is a power of two.
 * This enables us to implement a modulo operation in single bitwise
 * instruction. Check whether N_SHARDS is a number which is power of two is run
 * during the compile-time.
 * */
template <size_t N_SHARDS, typename Lock = std::shared_timed_mutex,
          template <typename...> class KeyValueImpl = std::unordered_map>
class Sharded_key_value_store
    : public Sharded_key_value_store_logger<Sharded_key_value_store<N_SHARDS>,
                                            DEBUG_BUILD> {
  /** Do not break encapsulation when using CRTP. */
  friend struct Sharded_key_value_store_logger<Sharded_key_value_store,
                                               DEBUG_BUILD>;

  /** Check whether all compile-time pre-conditions are set. */
  static_assert(N_SHARDS && !(N_SHARDS & (N_SHARDS - 1)),
                "N_SHARDS template parameter must be a power of two.");

  /** Bitmask which enables us to implement modulo-arithmetic operation in
   * single bitwise instruction. */
  static constexpr size_t MODULO_MASK = N_SHARDS - 1;

  /** In the event of inability to express ourselves with something like
   * std::array<alignas<N> Key_value_store<...>> we have to fallback to this
   * method.
   * */
  struct L1_dcache_aligned_kv_store {
    alignas(L1_DCACHE_SIZE) Key_value_store<Lock, KeyValueImpl> shard;
  };

  /** N_SHARDS instances of Key_value_store's. */
  std::array<L1_dcache_aligned_kv_store, N_SHARDS> m_kv_store_shard;

 public:
  /** Returns a reference to one of the underlying Key_value_store instances.
   *
   * [in] THD (connection) identifier.
   * @return A reference to Key_value_store instance. */
  Key_value_store<Lock, KeyValueImpl> &operator[](size_t thd_id) {
    return m_kv_store_shard[thd_id & MODULO_MASK].shard;
  }
};

}  // namespace temptable

#endif /* TEMPTABLE_SHARDED_KV_STORE_H */
