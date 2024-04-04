/*****************************************************************************

Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
#pragma once
#include <cstdint>
#include <limits>
#include "ut0bitset.h"
#include "ut0new.h"
namespace ut {
/** A Sharded_bitset<SHARDS_COUNT>(n) is like a bitset<n> in that it represents
a vector of n bits, which can be set(pos) or reset(pos) for 0<=pos<n.
One difference is that the size n is specified at runtime via constructor.
The other difference is that SHARDS_COUNT specifies sharding policy used and
enforced by the caller:

    shard_id = pos % SHARDS_COUNT

which is then respected by this class, by ensuring no data-race is possible, as
long as the caller ensures concurrent calls for two different pos from same
shard are not possible. For example, if one thread calls set(pos1) and another
calls set(pos2) in parallel, then it is fine, as long as

    pos1 % SHARDS_COUNT != pos2 % SHARDS_COUNT

This is the main reason to prefer this data structure to bitset, which doesn't
give you such guarantee as bit from different shards can be in the same word.
Another reason to use this data structure is because it has a very fast
implementation of find_set_in_this_shard(pos) which finds smallest
pos+SHARDS_COUNT*i which is set (for 0<=i). This is useful to quickly iterate
over all bits which are set, while also respecting sharding policy.
For example:
  for (size_t shard=0; shard < SHARDS_COUNT; ++shard) {
     mutex_enter(mutex[shard]);
     for (size_t pos = find_set_in_this_shard(shard);
         pos < n;
         pos = find_set_in_this_shard(pos+SHARDS_COUNT)) {

         cout << pos << " is set !" << endl;
     }
     mutex_exit(mutex[shard]);
  }
*/
template <size_t SHARDS_COUNT>
class Sharded_bitset {
 private:
  /** The bits for each shard are stored separately to avoid data-races,
  false-sharing, and make linear scans within shard faster.
  For similar reasons they are packed to aligned uint64_t words.
  But, they are allocated as one big array, so instead of indirect pointers,
  we can just compute where each shard starts or ends, @see get_shard() */
  ut::vector<uint64_t> words;
  using words_allocator = typename decltype(words)::allocator_type;

  /** How many words are devoted to each shard in words[]. All shards get equal
  fragment of words[], even if n is not divisible by SHARDS_COUNT, and thus the
  words_per_shard() might be slightly larger than necessary as we round up.
  The region of words[] for a given shard is
  words[shard*words_per_shard()]...words[(shard+1)*words_per_shard()-1]
  @return number of items of words[] assigned to each shard */
  size_t words_per_shard() const { return words.size() / SHARDS_COUNT; }

  /**
  @return a bitset wrapper around the fragment of words[] for specified shard */
  Bitset<> get_shard(size_t shard_id) const {
    const auto shard_bytes = words_per_shard() * 8;
    return Bitset<>((byte *)words.data() + shard_bytes * shard_id, shard_bytes);
  }

 public:
  /** Initializes a data structure capable of storing n bits.
  Initializes all bits to unset.
  @param[in]     n         The maximum position + 1.
  @param[in]     mem_key   PSI memory key used to track allocations. */
  Sharded_bitset(size_t n, ut::PSI_memory_key_t mem_key)
      : /* Each shard must have same length to keep the mapping simple.
        The shard to which maximal number of positions from [0,n) belongs is
        always the 0th shard, which needs to represent ceil(n/SHARDS_COUNT)
        bits. */
        words(ut::div_ceil(n, SHARDS_COUNT * 64) * SHARDS_COUNT, 0,
              words_allocator{mem_key.m_key}) {}

  /** Sets pos-th bit
  @param[in]     pos   The position of the bit to be set to 1 */
  void set(size_t pos) {
    get_shard(pos % SHARDS_COUNT).set(pos / SHARDS_COUNT);
  }

  /** Resets pos-th bit
  @param[in]     pos   The position of the bit to be reset to 0 */
  void reset(size_t pos) {
    get_shard(pos % SHARDS_COUNT).reset(pos / SHARDS_COUNT);
  }

  /** Finds a smallest position which is set and belongs to the same shard as
  start_pos, and is not smaller than start_pos
  @param[in]     start_pos   The position, such as passed to set(pos)
  @return Smallest start_pos+SHARDS_COUNT*i which is set and 0<=i. In case no
  bit set matching these criteria can be found, returns "infinity" */
  size_t find_set_in_this_shard(size_t start_pos) {
    const size_t shard_id = start_pos % SHARDS_COUNT;
    const auto bs = get_shard(shard_id);
    const size_t found = bs.find_set(start_pos / SHARDS_COUNT);
    return found == bs.NOT_FOUND ? found : found * SHARDS_COUNT + shard_id;
  }
};
}  // namespace ut
