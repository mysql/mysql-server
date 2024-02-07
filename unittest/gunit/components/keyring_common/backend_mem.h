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

#ifndef BACKEND_MEM_INCLUDED
#define BACKEND_MEM_INCLUDED

#include "components/keyrings/common/memstore/iterator.h"
#include "components/keyrings/common/operations/operations.h"
#include "components/keyrings/common/utils/utils.h"

namespace keyring_common_unit {
class Memory_backend final {
 public:
  Memory_backend() = default;

  virtual ~Memory_backend() = default;

  bool get(const keyring_common::meta::Metadata &metadata,
           keyring_common::data::Data &data) const {
    if (!metadata.valid()) return true;
    return !cache_.get(metadata, data);
  }

  bool store(const keyring_common::meta::Metadata &metadata,
             const keyring_common::data::Data &data) {
    if (!metadata.valid() || !data.valid()) return true;
    return !cache_.store(metadata, data);
  }

  bool erase(const keyring_common::meta::Metadata &metadata,
             const keyring_common::data::Data &data) {
    if (!metadata.valid()) return true;
    (void)data;
    return !cache_.erase(metadata);
  }

  bool generate(const keyring_common::meta::Metadata &metadata,
                keyring_common::data::Data &data, size_t length) {
    if (!metadata.valid()) return true;
    if (cache_.get(metadata, data)) return true;

    std::unique_ptr<unsigned char[]> key(new unsigned char[length]);
    if (!key) return true;
    if (!keyring_common::utils::get_random_data(key, length)) return true;

    std::string key_str;
    key_str.assign(reinterpret_cast<const char *>(key.get()), length);
    data.set_data(key_str);
    if (!cache_.store(metadata, data)) return true;

    return false;
  }

  bool load_cache(
      keyring_common::operations::Keyring_operations<Memory_backend> &) {
    return false;
  }

  size_t maximum_data_length() const { return 16384; }

  size_t size() const { return cache_.size(); }

 private:
  /** In memory cache for keyring data */
  keyring_common::cache::Datacache<keyring_common::data::Data> cache_;
};
}  // namespace keyring_common_unit

#endif  // !BACKEND_MEM_INCLUDED
