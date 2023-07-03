/*
   Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_VISIBLE_FIELDS_H
#define SQL_VISIBLE_FIELDS_H

/**
  @file
  An adapter class to support iteration over an iterator of Item *
  (typically mem_root_deque<Item *>), while skipping over items that
  are hidden (item->hidden == true). This is such a common operation
  that it warrants having its own adapter. You can either do

    for (Item *item : VisibleFields(fields))

  or use select->visible_fields().

  Behavior is undefined if you modify the hidden flag of an item during
  iteration.

  TODO(sgunders): When we get C++20, replace with an std::views::filter.
 */

#include <iterator>

#include "mem_root_deque.h"
#include "sql/item.h"

template <class Iterator>
class VisibleFieldsAdapter {
 public:
  VisibleFieldsAdapter(Iterator base, Iterator end) : m_it(base), m_end(end) {
    while (m_it != m_end && (*m_it)->hidden) {
      ++m_it;
    }
  }

  // Pre-increment.
  VisibleFieldsAdapter &operator++() {
    do {
      ++m_it;
    } while (m_it != m_end && (*m_it)->hidden);
    return *this;
  }

  // Post-increment.
  VisibleFieldsAdapter operator++(int) {
    VisibleFieldsAdapter ret = *this;
    do {
      m_it++;
    } while (m_it != m_end && (*m_it)->hidden);
    return ret;
  }

  auto &operator*() const { return *m_it; }

  bool operator==(const VisibleFieldsAdapter &other) const {
    return m_it == other.m_it;
  }
  bool operator!=(const VisibleFieldsAdapter &other) const {
    return m_it != other.m_it;
  }

  // Define aliases needed by std::iterator_traits, to allow using this iterator
  // type with standard library templates.
  using difference_type = typename Iterator::difference_type;
  using value_type = typename Iterator::value_type;
  using pointer = typename Iterator::pointer;
  using reference = typename Iterator::reference;
  using iterator_category = std::forward_iterator_tag;

 private:
  Iterator m_it, m_end;
};

template <class Container, class Iterator>
class VisibleFieldsContainer {
 public:
  explicit VisibleFieldsContainer(Container &fields) : m_fields(fields) {}
  VisibleFieldsAdapter<Iterator> begin() {
    return VisibleFieldsAdapter<Iterator>(m_fields.begin(), m_fields.end());
  }
  VisibleFieldsAdapter<Iterator> end() {
    return VisibleFieldsAdapter<Iterator>(m_fields.end(), m_fields.end());
  }

 private:
  Container &m_fields;
};

inline auto VisibleFields(mem_root_deque<Item *> &fields) {
  return VisibleFieldsContainer<mem_root_deque<Item *>,
                                mem_root_deque<Item *>::iterator>(fields);
}

inline auto VisibleFields(const mem_root_deque<Item *> &fields) {
  return VisibleFieldsContainer<const mem_root_deque<Item *>,
                                mem_root_deque<Item *>::const_iterator>(fields);
}

#endif  // SQL_VISIBLE_FIELDS_H
