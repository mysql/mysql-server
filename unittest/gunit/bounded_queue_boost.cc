/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "unittest/gunit/bounded_queue_boost.h"

#include <boost/heap/skew_heap.hpp>

#include "my_dbug.h"

namespace bh = boost::heap;

template <typename Key_type, typename Key_compare>
struct heap_data {
  typedef typename bh::skew_heap<heap_data, bh::mutable_<true>>::handle_type
      handle_type;

  handle_type m_handle;
  Key_type m_key;
  Key_compare m_cmp;

  heap_data(Key_type key, const Key_compare &cmp) : m_key(key), m_cmp(cmp) {}

  heap_data(const heap_data &that)
      : m_handle(that.m_handle), m_key(that.m_key), m_cmp(that.m_cmp) {}

  heap_data &operator=(const heap_data &that) {
    m_handle = that.m_handle;
    m_key = that.m_key;
    m_cmp = that.m_cmp;
    return *this;
  }

  bool operator<(const heap_data &rhs) const {
    return m_cmp(this->m_key, rhs.m_key);
  }
};

template <typename Element_type, typename Key_type, typename Key_generator,
          typename Key_compare>
class Bounded_queue_impl {
 public:
  typename bh::skew_heap<heap_data<Key_type, Key_compare>, bh::mutable_<true>>
      m_queue;
};

template <typename Element_type, typename Key_type, typename Key_generator,
          typename Key_compare>
Bounded_queue_boost<Element_type, Key_type, Key_generator,
                    Key_compare>::Bounded_queue_boost()
    : m_cmp(Key_compare()),
      m_sort_keys(NULL),
      m_compare_length(0),
      m_sort_param(NULL),
      m_max_elements(0),
      m_num_elements(0),
      pimpl(NULL) {}

template <typename Element_type, typename Key_type, typename Key_generator,
          typename Key_compare>
Bounded_queue_boost<Element_type, Key_type, Key_generator,
                    Key_compare>::~Bounded_queue_boost() {
  delete pimpl;
}

template <typename Element_type, typename Key_type, typename Key_generator,
          typename Key_compare>
int Bounded_queue_boost<Element_type, Key_type, Key_generator,
                        Key_compare>::init(ha_rows max_elements,
                                           Key_generator *sort_param,
                                           Key_type *sort_keys) {
  m_sort_keys = sort_keys;
  m_compare_length = sort_param->max_compare_length();
  m_sort_param = sort_param;

  m_max_elements = max_elements + 1;
  pimpl = new Bounded_queue_impl<Element_type, Key_type, Key_generator,
                                 Key_compare>();
  return 0;
}

template <typename Element_type, typename Key_type, typename Key_generator,
          typename Key_compare>
void Bounded_queue_boost<Element_type, Key_type, Key_generator,
                         Key_compare>::push(Element_type element) {
  DBUG_ASSERT(pimpl != NULL);
  if (m_num_elements == m_max_elements) {
    const heap_data<Key_type, Key_compare> &pq_top = pimpl->m_queue.top();
    m_sort_param->make_sortkey(pq_top.m_key, element);
    pimpl->m_queue.update(pq_top.m_handle);
  } else {
    m_sort_param->make_sortkey(m_sort_keys[m_num_elements], element);
    typename heap_data<Key_type, Key_compare>::handle_type handle =
        pimpl->m_queue.push(heap_data<Key_type, Key_compare>(
            m_sort_keys[m_num_elements], m_cmp));
    ++m_num_elements;
    (*handle).m_handle = handle;
  }
}
