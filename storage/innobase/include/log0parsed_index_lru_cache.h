/* Copyright (c) 2026, Oracle and/or its affiliates.

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
*/

#include <memory>
#include <string>
#include <string_view>

#include "dict0mem.h"
#include "ut0prefix_cache.h"

/** Parsed index cache for redo parsing.

This wrapper binds ut::prefix_cache<byte, Value> to the redo parsing use
case where Value is a unique_ptr owning dict_index_t with custom cleanup. */
class Parsed_index_lru_cache {
 public:
  /** A single dict_index_t + dict_table_t takes several kilobytes.
  InnoDB recovery logic needs one cache for the main thread doing the parsing,
  and one for each of srv_n_read_io_threads. */
  static constexpr size_t k_capacity = 100;

  using sequence = std::basic_string<byte>;
  using sequence_view = std::basic_string_view<byte>;

  Parsed_index_lru_cache() : m_cache(k_capacity) {}
  Parsed_index_lru_cache(const Parsed_index_lru_cache &) = delete;
  Parsed_index_lru_cache &operator=(const Parsed_index_lru_cache &) = delete;

  dict_index_t *lookup(sequence_view &buffer) {
    Value *cached = m_cache.find_value_for_prefix_of(buffer);
    if (cached == nullptr) {
      return nullptr;
    }
    return cached->get();
  }

  bool insert_mru(sequence_view key, dict_index_t *index) {
    return m_cache.insert(key, Value(index));
  }

 private:
  struct Index_deleter {
    void operator()(dict_index_t *idx) const {
      if (idx == nullptr) {
        return;
      }

      dict_table_t *table = idx->table;
      dict_mem_index_free(idx);
      dict_mem_table_free(table);
    }
  };

  using Value = std::unique_ptr<dict_index_t, Index_deleter>;

 private:
  ut::prefix_cache<byte, Value> m_cache;
};
