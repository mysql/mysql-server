#ifndef JSON_HASH_INCLUDED
#define JSON_HASH_INCLUDED

/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <assert.h>
#include <stddef.h>
#include <iterator>
#include <map>
#include <memory>  // unique_ptr
#include <new>
#include <string>
#include <string_view>
#include <type_traits>  // is_base_of
#include <utility>
#include <vector>

#include "extra/xxhash/my_xxhash.h"
#include "sql-common/json_dom.h"

/// Helper class for building a hash key. This class can be used to plugin any
/// individual hash algorithm by overriding add_character() and add_string()
/// functions.
class Json_wrapper_hasher {
 public:
  virtual void add_character(uchar ch) = 0;

  void add_integer(longlong ll) {
    char tmp[8];
    int8store(tmp, ll);
    add_string(tmp, sizeof(tmp));
  }

  void add_double(double d) {
    // Make -0.0 and +0.0 have the same key.
    if (d == 0) {
      add_character(0);
      return;
    }

    char tmp[8];
    float8store(tmp, d);
    add_string(tmp, sizeof(tmp));
  }

  virtual void add_string(const char *str, size_t len) = 0;

  /**
    Return the computed hash value in integer form. This is optional as each
    seperate hasher would like to produce hash in different forms.
  */
  virtual ulonglong get_hash_value() { return 0; }

  virtual ~Json_wrapper_hasher() = default;
};

/// Helper class for building a hash key.
class Json_wrapper_crc_hasher : public Json_wrapper_hasher {
 private:
  ulonglong m_crc;

 public:
  explicit Json_wrapper_crc_hasher(ulonglong hash_val) : m_crc(hash_val) {}

  ulonglong get_hash_value() override { return m_crc; }

  void add_character(uchar ch) override { add_to_crc(ch); }

  void add_string(const char *str, size_t len) override {
    for (size_t idx = 0; idx < len; idx++) {
      add_to_crc(*str++);
    }
  }

 private:
  /**
    Add another character to the evolving crc.

    @param[in] ch The character to add
  */
  void add_to_crc(uchar ch) {
    // This logic was cribbed from sql_executor.cc/unique_hash
    m_crc = ((m_crc << 8) + (((uchar)ch))) +
            (m_crc >> (8 * sizeof(ha_checksum) - 8));
  }
};

class Json_wrapper_xxh_hasher : public Json_wrapper_hasher {
  XXH3_state_t *state = nullptr;

 public:
  explicit Json_wrapper_xxh_hasher() : state(XXH3_createState()) {
    XXH3_128bits_reset(this->state);
  }
  void add_string(const char *str, size_t len) override {
    if (len == 0) {
      add_character('\0');
    } else {
      XXH3_128bits_update(this->state, str, len);
    }
  }
  void reset() { XXH3_128bits_reset(this->state); }
  XXH128_hash_t get_digest() { return XXH3_128bits_digest(this->state); }
  void add_character(uchar ch) override {
    XXH3_128bits_update(this->state, &ch, 1);
  }
  ~Json_wrapper_xxh_hasher() override { XXH3_freeState(this->state); }
};

constexpr uint HEX_ENC_ETAG_SIZE = 16;

bool calculate_etag_for_json(
    const Json_wrapper &wr, Json_wrapper_hasher &hash_key,
    const JsonSerializationErrorHandler &error_handler,
    const std::unordered_set<std::string> *json_arrayagg_keys = nullptr,
    std::string *path = nullptr);

void XXH128_hash_hex(XXH128_hash_t h, String *s);

XXH128_hash_t add_xxh128_hash(XXH128_hash_t l, XXH128_hash_t r);

#endif
