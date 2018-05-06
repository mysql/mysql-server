/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef X_CLIENT_XPRIORITY_LIST_H_
#define X_CLIENT_XPRIORITY_LIST_H_

#include <algorithm>
#include <list>

namespace xcl {

template <typename Element_type,
          bool (*Priority_compare)(const Element_type &, const Element_type &) =
              Element_type::compare>
class Priority_list {
 public:
  using Iterator = typename std::list<Element_type>::iterator;

 public:
  void push_front(const Element_type &n) {
    const auto i = std::find_if(
        m_list.begin(), m_list.end(),
        [&n](const Element_type &e) { return !Priority_compare(e, n); });

    if (m_list.end() == i) {
      m_list.push_back(n);

      return;
    }

    m_list.insert(i, n);
  }

  void push_back(const Element_type &n) {
    const auto i = std::find_if(
        m_list.rbegin(), m_list.rend(),
        [&n](const Element_type &e) { return !Priority_compare(n, e); });

    if (m_list.rend() == i) {
      m_list.push_front(n);

      return;
    }

    m_list.insert(i.base(), n);
  }

  void erase(Iterator i) { m_list.erase(i); }

  void clear() { m_list.clear(); }

  Iterator begin() { return m_list.begin(); }

  Iterator end() { return m_list.end(); }

 private:
  std::list<Element_type> m_list;
};

}  // namespace xcl

#endif  // X_CLIENT_XPRIORITY_LIST_H_
