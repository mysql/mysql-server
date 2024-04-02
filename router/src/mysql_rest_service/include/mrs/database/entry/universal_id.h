/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_UNIVERSAL_ID_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_UNIVERSAL_ID_H_

#include <string.h>
#include <algorithm>
#include <cassert>
#include <optional>
#include <utility>

#include "helper/string/hex.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {
namespace entry {

struct UniversalId {
  constexpr static uint64_t k_size = 16;

  UniversalId() {
    memset(raw, 0, k_size);
    //      num.higher_id = 0;
    //      num.lower_id = 0;
  }

  UniversalId(std::initializer_list<uint8_t> v) {
    assert(v.size() <= sizeof(raw));
    memset(raw, 0, sizeof(raw));
    std::copy_n(v.begin(), std::min(v.size(), sizeof(raw)), std::begin(raw));
  }

  UniversalId(const UniversalId &id) { *this = id; }

  uint8_t raw[k_size];

  const uint8_t *begin() const { return std::begin(raw); }
  const uint8_t *end() const { return std::end(raw); }

  void operator=(const UniversalId &other) { memcpy(raw, other.raw, k_size); }

  bool operator==(const UniversalId &other) const {
    return 0 == memcmp(raw, other.raw, k_size);
  }

  bool operator!=(const UniversalId &other) const { return !(*this == other); }

  bool operator<(const UniversalId &other) const {
    for (int i = k_size - 1; i >= 0; --i) {
      if (raw[i] != other.raw[i]) return raw[i] < other.raw[i];
    }

    return false;
  }

  bool operator>(const UniversalId &other) const {
    for (int i = k_size - 1; i >= 0; --i) {
      if (raw[i] != other.raw[i]) return raw[i] > other.raw[i];
    }

    return false;
  }

  static UniversalId from_cstr(const char *p, uint32_t length) {
    if (length != k_size) return {};
    UniversalId result;
    from_raw(&result, p);
    return result;
  }

  const char *to_raw() const { return reinterpret_cast<const char *>(raw); }

  static void from_raw(UniversalId *uid, const char *binray) {
    memcpy(uid->raw, binray, k_size);
  }

  static void from_raw_zero_on_null(UniversalId *uid, const char *binray) {
    if (binray)
      memcpy(uid->raw, binray, k_size);
    else
      memset(uid->raw, 0, k_size);
  }

  static void from_raw_optional(std::optional<UniversalId> *uid,
                                const char *binray) {
    if (binray) {
      UniversalId result;
      from_raw(&result, binray);
      *uid = std::move(result);
    } else {
      (*uid).reset();
    }
  }

  std::string to_string() const { return helper::string::hex(raw); }
};

inline mysqlrouter::sqlstring to_sqlstring(const UniversalId &ud) {
  mysqlrouter::sqlstring result{"X?"};
  result << ud.to_string();
  return result;
}

inline std::string to_string(const UniversalId &ud) { return ud.to_string(); }

inline mysqlrouter::sqlstring &operator<<(mysqlrouter::sqlstring &sql,
                                          const UniversalId &ud) {
  sql << to_sqlstring(ud);

  return sql;
}

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_UNIVERSAL_ID_H_
