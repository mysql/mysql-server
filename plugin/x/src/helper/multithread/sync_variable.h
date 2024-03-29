/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_HELPER_MULTITHREAD_SYNC_VARIABLE_H_
#define PLUGIN_X_SRC_HELPER_MULTITHREAD_SYNC_VARIABLE_H_

#include <algorithm>
#include <atomic>
#include <iterator>

#include "plugin/x/src/helper/multithread/synchronize.h"

namespace xpl {

template <typename Variable_type>
class Sync_variable {
 public:
  Sync_variable(const Variable_type value, const PSI_mutex_key mutex_key,
                const PSI_cond_key cond_key)
      : m_value(value), m_sync(mutex_key, cond_key) {}

  bool is(const Variable_type value_to_check) const {
    return value_to_check == m_value.load();
  }

  const Variable_type get() const { return m_value.load(); }

  template <std::size_t NUM_OF_ELEMENTS>
  bool is(const Variable_type (&expected_value)[NUM_OF_ELEMENTS]) const {
    auto sync = m_sync.block();

    const Variable_type *begin_element = expected_value;
    const Variable_type *end_element = expected_value + NUM_OF_ELEMENTS;

    return end_element != std::find(begin_element, end_element, m_value);
  }

  bool exchange(const Variable_type expected_value,
                const Variable_type new_value) {
    auto sync = m_sync.block();

    bool result = false;

    if (expected_value == m_value) {
      m_value = new_value;

      sync.notify();
      result = true;
    }

    return result;
  }

  void set(const Variable_type new_value) {
    auto sync = m_sync.block();

    m_value = new_value;

    sync.notify();
  }

  Variable_type set_and_return_old(const Variable_type new_value) {
    auto sync = m_sync.block();

    Variable_type old_value = m_value;
    m_value = new_value;

    sync.notify();

    return old_value;
  }

  void wait_for(const Variable_type expected_value) const {
    auto sync = m_sync.block();

    while (m_value != expected_value) {
      sync.wait();
    }
  }

  template <std::size_t NUM_OF_ELEMENTS>
  Variable_type wait_for(
      const Variable_type (&expected_values)[NUM_OF_ELEMENTS]) const {
    auto sync = m_sync.block();

    return wait_for_impl(&sync, expected_values);
  }

  template <std::size_t NUM_OF_ELEMENTS>
  Variable_type wait_for_and_set(
      const Variable_type (&expected_values)[NUM_OF_ELEMENTS],
      const Variable_type change_to) {
    auto sync = m_sync.block();
    const auto result = wait_for_impl(&sync, expected_values);

    if (change_to != m_value) {
      m_value = change_to;
      sync.notify();
    }

    return result;
  }

 protected:
  template <std::size_t NUM_OF_ELEMENTS>
  Variable_type wait_for_impl(
      Synchronize::Block *sync,
      const Variable_type (&expected_values)[NUM_OF_ELEMENTS]) const {
    while (std::none_of(
        std::begin(expected_values), std::end(expected_values),
        [this](const Variable_type value) { return m_value == value; })) {
      sync->wait();
    }

    return m_value;
  }

  std::atomic<Variable_type> m_value;
  mutable Synchronize m_sync;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_MULTITHREAD_SYNC_VARIABLE_H_
