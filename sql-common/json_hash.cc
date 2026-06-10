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

#include "sql-common/json_hash.h"

#include <cassert>
#include <cstddef>
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
#include "sql/item_strfunc.h"

#ifdef MYSQL_SERVER
#include "sql/current_thd.h"
#endif

XXH128_hash_t add_xxh128_hash(XXH128_hash_t l, XXH128_hash_t r) {
  XXH128_hash_t a;
  a.low64 = (l.low64 + r.low64);
  auto carry = static_cast<XXH64_hash_t>((l.low64 + r.low64) < l.low64);
  a.high64 = (l.high64 + r.high64 + carry);
  return a;
}

void XXH128_hash_hex(XXH128_hash_t h, String *s) {
  char hexBuffer[2 * HEX_ENC_ETAG_SIZE];
  tohex(hexBuffer, h.high64, HEX_ENC_ETAG_SIZE);
  tohex(hexBuffer + HEX_ENC_ETAG_SIZE, h.low64, HEX_ENC_ETAG_SIZE);
  s->append(hexBuffer, 2 * HEX_ENC_ETAG_SIZE);
}

bool calculate_etag_for_json(
    const Json_wrapper &wr, Json_wrapper_hasher &hash_key,
    const JsonSerializationErrorHandler &error_handler,
    const std::unordered_set<std::string> *json_arrayagg_keys,
    std::string *path) {
  if (error_handler.CheckStack()) {
    return true;
  }

  switch (wr.type()) {
    case enum_json_type::J_OBJECT: {
      hash_key.add_character(JSON_KEY_OBJECT);
      for (const auto &it : Json_object_wrapper(wr)) {
        hash_key.add_string(it.first.data(), it.first.size());
        std::string prev_path;
        // apending last leg
        if (path != nullptr) {
          prev_path = std::string(*path);
          path->append(".\"" + std::string(it.first.data(), it.first.size()) +
                       "\"");
        }
        calculate_etag_for_json(it.second, hash_key, error_handler,
                                json_arrayagg_keys, path);
        // removing the last leg
        if (path != nullptr) {
          path->clear();
          path->append(prev_path);
        }
      }
      break;
    }
    case enum_json_type::J_ARRAY: {
      bool ignore_elements_order =
          json_arrayagg_keys != nullptr &&
          json_arrayagg_keys->find(*path) != json_arrayagg_keys->end();

      hash_key.add_character(JSON_KEY_ARRAY);
      size_t array_elements = wr.length();
      if (ignore_elements_order) {
        XXH128_hash_t hash_of_hash;
        hash_of_hash.low64 = 0;
        hash_of_hash.high64 = 0;
        std::string prev_path;
        // apending last leg
        if (path != nullptr) {
          prev_path = std::string(*path);
          path->append("[*]");
        }
        Json_wrapper_xxh_hasher hash_key_ele;
        for (uint i = 0; i < array_elements; i++) {
          hash_key_ele.reset();
          calculate_etag_for_json(wr[i], hash_key_ele, error_handler,
                                  json_arrayagg_keys, path);
          hash_of_hash =
              add_xxh128_hash(hash_of_hash, hash_key_ele.get_digest());
        }
        // removing the last leg
        if (path != nullptr) {
          path->clear();
          path->append(prev_path);
        }
        hash_key.add_integer(hash_of_hash.low64);
        hash_key.add_integer(hash_of_hash.high64);
      } else {
        for (uint i = 0; i < array_elements; i++) {
          calculate_etag_for_json(wr[i], hash_key, error_handler);
        }
      }
      break;
    }
    case enum_json_type::J_ERROR:
      assert(false); /* purecov: inspected */
      return true;   /* purecov: inspected */
    default:
      wr.make_hash_key_common(hash_key);
  }
  return false;
}
