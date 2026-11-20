/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

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

#include "sql/common/component.h"
#include <boost/algorithm/string/case_conv.hpp>

#include "sql/item.h"
#include "sql/item_timefunc.h"
#include "sql/sql_class.h"

namespace im {

template <typename F, typename S>
bool Pair_key_comparator<F, S>::operator()(
    const Pair_key_type<F, S> &lhs, const Pair_key_type<F, S> &rhs) const {
  return (strcmp(lhs.first.c_str(), rhs.first.c_str()) == 0 &&
          strcmp(lhs.second.c_str(), rhs.second.c_str()) == 0);
}

template bool Pair_key_comparator<std::string, std::string>::operator()(
    const Pair_key_type<std::string, std::string> &lhs,
    const Pair_key_type<std::string, std::string> &rhs) const;

/* String compare ignoring case */
template <typename F, typename S>
bool Pair_key_icase_comparator<F, S>::operator()(
    const Pair_key_type<F, S> &lhs, const Pair_key_type<F, S> &rhs) const {
  F l_f = lhs.first;
  S l_s = lhs.second;
  F r_f = rhs.first;
  S r_s = rhs.second;
  boost::algorithm::to_upper(l_f);
  boost::algorithm::to_upper(l_s);
  boost::algorithm::to_upper(r_f);
  boost::algorithm::to_upper(r_s);
  return (l_f.compare(r_f) == 0 && l_s.compare(r_s) == 0);
}

template bool Pair_key_icase_comparator<std::string, std::string>::operator()(
    const Pair_key_type<std::string, std::string> &lhs,
    const Pair_key_type<std::string, std::string> &rhs) const;

template <typename F, typename S>
size_t Pair_key_icase_hash<F, S>::operator()(
    const im::Pair_key_type<F, S> &p) const {
  F s1 = p.first;
  S s2 = p.second;
  boost::algorithm::to_upper(s1);
  boost::algorithm::to_upper(s2);
  return std::hash<F>()(static_cast<const F>(s1)) ^
         std::hash<S>()(static_cast<const S>(s2));
}

template size_t Pair_key_icase_hash<std::string, std::string>::operator()(
    const im::Pair_key_type<std::string, std::string> &p) const;

} /* namespace im */

namespace std {
template <typename F, typename S>
size_t hash<im::Pair_key_type<F, S>>::operator()(
    const im::Pair_key_type<F, S> &p) const {
  return hash<F>()(static_cast<const F>(p.first)) ^
         hash<S>()(static_cast<const S>(p.second));
}

template size_t hash<im::Pair_key_type<std::string, std::string>>::operator()(
    const im::Pair_key_type<std::string, std::string> &p) const;

} /* namespace std */

static bool is_string_item(Item *item) {
  switch (item->data_type()) {
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
      return true;
    default:
      return false;
  }
}

bool try_cast_to_datetime(THD *thd, Item **item) {
  Item *cast;

  /* String could be casted to datetime */
  if (!is_string_item(*item)) return true;

  assert(thd);
  if (!(cast = new (thd->mem_root) Item_typecast_datetime(*item, true)))
    return true;

  /* If cannot cast, errors will be raised when evaluating */
  cast->fix_fields(thd, item);
  /* No need to register to the thd->change_list */
  *item = cast;

  return false;
}
