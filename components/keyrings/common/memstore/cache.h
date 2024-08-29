/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef CACHE_INCLUDED
#define CACHE_INCLUDED

#include <unordered_map>
#include <utility>

#include "components/keyrings/common/data/data.h" /* data::Data */
#include "components/keyrings/common/data/meta.h" /* meta::Metadata */

namespace keyring_common::cache {

template <typename Data_extension>
using Cache =
    std::unordered_map<meta::Metadata, Data_extension, meta::Metadata::Hash>;

template <typename Data_extension = data::Data>
class Datacache final {
 public:
  /** Constructor */
  explicit Datacache() { version_ = 0; }

  /** Disable copy */
  Datacache(const Datacache &src) = delete;
  Datacache(Datacache &&src) = delete;
  Datacache &operator=(const Datacache &src) = delete;
  Datacache &operator=(Datacache &&src) = delete;

  /** Destructor */
  ~Datacache() = default;

  /**
    Swap content of two caches.

    @param [in, out] a   first cache to be swapped
    @param [in, out] b   second cache to be swapped
  */
  static void swap(Datacache &a, Datacache &b) {
    std::swap(a.version_, b.version_);
    std::swap(a.cache_, b.cache_);
  }

  /**
    Retrieve an element from cache
    @param [in]  metadata Key to search data
    @param [out] data     Fetched data. Can be empty.

    @returns status of find operation
      @retval true  Success. data contains retrieved data.
      @retval false Failure. data may not contain a valid value.
  */
  bool get(const meta::Metadata metadata, Data_extension &data) const {
    auto it = cache_.find(metadata);
    if (it == cache_.end()) return false;
    data = it->second;
    return true;
  }

  /**
    Store and element in cache
    @param [in] metadata Key to store data
    @param [in] data     Actual data. Can be empty.

    @returns status of insert operation
      @retval true  Success
      @retval false Error. Element already exists in the cache.
  */
  bool store(const meta::Metadata metadata, const Data_extension data) {
    bool ok = cache_.insert({metadata, data}).second;
    if (ok) ++version_;
    return ok;
  }

  /**
    Remove an entry from cache
    @param [in] metadata Key to entry to be erased

    @returns status of find operation
      @retval true  Success. Data removed successfully.
      @retval false Failure. Either key is not present or removal failed.
  */
  bool erase(const meta::Metadata metadata) {
    bool removed = cache_.erase(metadata) != 0;
    if (removed) ++version_;
    return removed;
  }

  /** Clear the cache */
  void clear() { cache_.clear(); }

  /** Check if cache is empty */
  bool empty() const { return cache_.empty(); }

  /** Get size */
  size_t size() const { return cache_.size(); }

  /** Get cache version */
  size_t version() const { return version_; }

  /* Iterators */
  typename Cache<Data_extension>::const_iterator begin() const {
    return cache_.cbegin();
  }
  typename Cache<Data_extension>::const_iterator end() const {
    return cache_.cend();
  }

  /**
    Retrieve iterator at an element from cache
    @param [in]  metadata Key to search data
  */
  typename Cache<Data_extension>::const_iterator at(
      const meta::Metadata metadata) const {
    return cache_.find(metadata);
  }

 private:
  /** Sensitive data cache */
  Cache<Data_extension> cache_;
  /** Cache version */
  size_t version_{0};
};

}  // namespace keyring_common::cache

#endif  // !CACHE_INCLUDED
