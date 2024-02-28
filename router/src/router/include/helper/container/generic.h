/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_ROUTER_INCLUDE_HELPER_CONTAINER_GENERIC_H_
#define ROUTER_SRC_ROUTER_INCLUDE_HELPER_CONTAINER_GENERIC_H_

#include <algorithm>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace helper {
namespace container {

template <typename Container, typename Value = typename Container::value_type>
typename Container::const_iterator find(const Container &c, Value &&value) {
  return std::find(c.begin(), c.end(), std::forward<Value>(value));
}

template <typename Container, typename Value = typename Container::value_type>
bool remove(Container &c, Value &&value) {
  auto it = find(c, std::forward<Value>(value));

  if (it == c.end()) return false;

  c.erase(it);
  return true;
}

template <typename Container, typename Find_if>
typename Container::const_iterator find_if(const Container &c,
                                           Find_if &&find_if) {
  return std::find_if(c.begin(), c.end(), std::forward<Find_if>(find_if));
}

template <typename Container, typename Find_if>
bool remove_if(Container &c, Find_if &&value) {
  auto it = find_if<Container, Find_if>(c, std::forward<Find_if>(value));

  if (it == c.end()) return false;

  c.erase(it);
  return true;
}

template <typename Container, typename Find_if>
bool get_ptr_if(const Container &c, Find_if &&find_if,
                const typename Container::value_type **out) {
  auto it = std::find_if(c.begin(), c.end(), std::forward<Find_if>(find_if));
  if (c.end() == it) return false;
  *out = &(*it);
  return true;
}

template <typename Container, typename Find_if>
bool get_if(const Container &c, Find_if &&find_if,
            const typename Container::value_type *out) {
  auto it = std::find_if(c.begin(), c.end(), std::forward<Find_if>(find_if));
  if (c.end() == it) return false;
  *out = (*it);
  return true;
}

template <typename Container, typename Find_if>
bool get_if(Container &c, Find_if &&find_if,
            typename Container::value_type *out) {
  auto it = std::find_if(c.begin(), c.end(), std::forward<Find_if>(find_if));
  if (c.end() == it) return false;
  *out = (*it);
  return true;
}

template <typename Container, typename Value = typename Container::value_type>
bool has(const Container &c, Value &&val) {
  auto b = std::begin(c);
  auto e = std::end(c);
  auto it = std::find(b, e, std::forward<Value>(val));
  if (e == it) return false;
  return true;
}

template <typename Container, typename Value = typename Container::value_type>
int index_of(Container &c, Value &&val) {
  auto it = std::find(c.begin(), c.end(), std::forward<Value>(val));
  if (c.end() == it) return -1;
  return std::distance(c.begin(), it);
}

template <typename Container, typename Find_if>
void copy_if(const Container &input, Find_if &&find_if, Container &output) {
  for (auto &e : input) {
    if (find_if(e)) output.push_back(e);
  }
}

template <typename Container, typename Value = typename Container::value_type>
std::vector<Value> as_vector(const Container &v) {
  return std::vector<Value>(v.begin(), v.end());
}

template <typename Value = uint8_t, typename Container>
std::vector<Value> as_vector_t(const Container &v) {
  return as_vector<Container, Value>(v);
}

template <typename Container, typename Value = typename Container::value_type>
std::set<Value> as_set(const Container &v) {
  return std::set<Value>(v.begin(), v.end());
}

template <typename Value = uint8_t, typename Container>
std::set<Value> as_set_t(const Container &v) {
  return as_set<Container, Value>(v);
}

}  // namespace container
}  // namespace helper

#endif  // ROUTER_SRC_ROUTER_INCLUDE_HELPER_CONTAINER_GENERIC_H_
