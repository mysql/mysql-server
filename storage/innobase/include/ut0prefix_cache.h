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
#pragma once

#include <functional>
#include <list>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "univ.i"

namespace ut {

/** Bounded cache for prefix lookups with MRU/LRU recency tracking.

The main use case is in parsers, which consume a prefix of a buffer, to produce
a value, where the length of the prefix is not know up-front without parsing.
This solves the case in which callers need to map owned prefixes to values,
probe whether a cached prefix matches the beginning of a buffer, and evict old
entries when the cache reaches a fixed size. Standard containers alone are not
enough, because they do not combine prefix lookup, stable owned keys, and LRU
eviction in one reusable abstraction.

Important limitation of this class (met by the parser usecase) is that it can
work only with prefix-free languages, i.e. that is it doesn't support adding two
prefixes such that one is a proper prefix of another. */
template <typename CharType, typename Value>
class prefix_cache {
 public:
  using sequence = std::basic_string<CharType>;
  using sequence_view = std::basic_string_view<CharType>;

  /** Create a cache with at most @p capacity stored prefixes.

  @param[in] capacity The maximum number of entries kept after each insertion */
  explicit prefix_cache(size_t capacity) : m_capacity(capacity) {}

  prefix_cache(const prefix_cache &) = delete;
  prefix_cache &operator=(const prefix_cache &) = delete;

  /** Find a cached value whose key is the longest prefix of @p buffer.

  If a match is found, the entry becomes most recently used and the buffer
  is trimmed to the leftover suffix.

  @param[in,out] buffer
                    The input buffer. Trimmed to the leftover suffix on match.
                    Otherwise kept intact.
  @return The pointer to cached value or nullptr if no match. It is only safe
  to dereference it until next call to insert(..). */
  Value *find_value_for_prefix_of(sequence_view &buffer) {
    const It it = try_find(buffer);
    if (it == m_map.end()) {
      return nullptr;
    }

    const size_t matched_len = it->first.size();
    move_to_front(it);
    buffer.remove_prefix(matched_len);

    return &it->second.value;
  }

  /** Insert a new prefix/value pair as the most recently used entry.

  If @p prefix already exists, the cache is left unchanged. After a successful
  insertion, least recently used entries are evicted until the cache size is
  within capacity.

  The caller must ensure that the language is prefix-free, i.e. that the prefix
  is not a proper prefix of any previously inserted one, nor vice-versa.

  Invalidates pointers returned by find_value_for_prefix_of(..), as they might
  become evicted as a result of this call.

  @param[in] prefix key prefix.
  @param[in] value value to store.
  @return true if inserted, false if key already existed */
  bool insert(sequence_view prefix, Value value) {
    if (m_map.contains(prefix)) {
      return false;
    }

    auto [it, inserted] =
        m_map.emplace(sequence(prefix), Entry{std::move(value), m_lru.end()});
    ut_a(inserted);
#ifdef UNIV_DEBUG
    /* This class only works for prefix-free languages. If that assumption was
    violated, i.e. if one key was a proper prefix of another, then there would
    also exist such a pair where they are next to each other, and it had to form
    after one of the insert operations. So, we check here both neighbours, to
    validate this contract is not violated. */
    if (it != m_map.begin()) {
      auto prev = it;
      --prev;
      ut_ad(!is_prefix(prefix, prev->first));
    }
    auto next = it;
    ++next;
    if (next != m_map.end()) {
      ut_ad(!is_prefix(next->first, prefix));
    }
#endif /* UNIV_DEBUG */

    link_new_mru(it);
    enforce_capacity();

    return true;
  }

 private:
  struct Entry;
  /* We use greater so that if A is a prefix of B, then
  B is before A in the ordering, and hence lower_bound(B) can find A.
  This also means it finds the longest prefix. */
  using Map = std::map<sequence, Entry, std::greater<>>;
  using It = typename Map::iterator;
  using Lru_list = std::list<It>;
  using Lru_it = typename Lru_list::iterator;

  struct Entry {
    Value value;
    Lru_it lru_pos;
  };

  static bool is_prefix(sequence_view key, sequence_view buffer) {
    return buffer.size() >= key.size() &&
           std::char_traits<CharType>::compare(key.data(), buffer.data(),
                                               key.size()) == 0;
  }

  It try_find(sequence_view buffer) {
    const It it = m_map.lower_bound(buffer);
    if (it != m_map.end() && is_prefix(it->first, buffer)) {
      return it;
    }
    return m_map.end();
  }

  void move_to_front(It it) {
    ut_ad(it != m_map.end());
    m_lru.splice(m_lru.begin(), m_lru, it->second.lru_pos);
    it->second.lru_pos = m_lru.begin();
  }

  void link_new_mru(It it) {
    m_lru.push_front(it);
    it->second.lru_pos = m_lru.begin();
  }

  void enforce_capacity() {
    while (m_map.size() > m_capacity) {
      ut_ad(!m_lru.empty());
      It victim = m_lru.back();
      m_lru.pop_back();
      m_map.erase(victim);
    }
  }

 private:
  size_t m_capacity;
  Map m_map;
  Lru_list m_lru;
};

}  // namespace ut
