/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_INSERT_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_INSERT_H_

#include <string>
#include <string_view>

#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryRestObjectInsert : private QueryLog {
 private:
  template <typename K, typename V>
  class It {
   public:
    It(K k, V v) : k_{k}, v_{v} {}

    It &operator++() {
      ++k_;
      ++v_;
      return *this;
    }

    auto operator*() {
      mysqlrouter::sqlstring r{" !=?"};
      r << *k_ << *v_;
      return r;
    }

    bool operator!=(const It &other) { return k_ != other.k_; }

   private:
    K k_;
    V v_;
  };

  mysqlrouter::sqlstring additional_where(
      const std::string &user_key, const mysqlrouter::sqlstring &user_value) {
    if (user_key.empty()) return {};

    mysqlrouter::sqlstring where{"AND !=?"};
    where << user_key << user_value;
    return where;
  }

 public:
  template <typename KeysIt, typename ValuesIt>
  void execute_insert(MySQLSession *session, const std::string &schema,
                      const std::string &object, const KeysIt &kit,
                      const ValuesIt &vit) {
    query_ = {"INSERT INTO !.!(!) VALUES(?)"};
    query_ << schema << object << kit << vit;
    execute(session);
    affected = 1;
  }

  template <typename KeysIt, typename ValuesIt>
  bool update(MySQLSession *session, const std::string &schema,
              const std::string &object, const KeysIt &kit, const ValuesIt &vit,
              const std::string &pk, const mysqlrouter::sqlstring &pk_value,
              const std::string &user_key,
              const mysqlrouter::sqlstring &user_value) {
    affected = 0;
    using ItTypes =
        It<typename KeysIt::first_type, typename ValuesIt::first_type>;

    query_ = {"UPDATE !.! SET ? WHERE !=? ?"};
    using ItTypes =
        It<typename KeysIt::first_type, typename ValuesIt::first_type>;
    query_ << schema << object
           << std::make_pair<ItTypes, ItTypes>(ItTypes(kit.first, vit.first),
                                               ItTypes(kit.second, vit.second))
           << pk << pk_value << additional_where(user_key, user_value);
    execute(session);
    affected = session->affected_rows();
    return 0 != affected;
  }

  uint64_t affected;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_INSERT_H_
