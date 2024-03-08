/*  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
#ifndef ROUTER_SRC_ROUTER_INCLUDE_HELPER_WAIT_VARIABLE_H_
#define ROUTER_SRC_ROUTER_INCLUDE_HELPER_WAIT_VARIABLE_H_

#include <initializer_list>

#include "container/generic.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/monitor.h"

template <typename ValueType>
class WaitableVariable {
 public:
  WaitableVariable() {}
  WaitableVariable(const ValueType &value) : monitor_with_value_{value} {}

  class DoNothing {
   public:
    void operator()() const {}
  };

  template <typename Container = std::initializer_list<ValueType>,
            typename Callback = DoNothing>
  bool exchange(const Container &expected, const ValueType &v,
                const Callback &after_set_callback = DoNothing()) {
    bool result{false};
    monitor_with_value_.serialize_with_cv(
        [&expected, &v, &after_set_callback, &result](auto &value, auto &cv) {
          if (helper::container::has(expected, value)) {
            value = v;
            result = true;
            after_set_callback();
            cv.notify_all();
          }
        });

    return result;
  }

  template <typename Callback = DoNothing>
  bool exchange(const ValueType &expected, const ValueType &v,
                const Callback &after_set_callback = DoNothing()) {
    bool result{false};
    monitor_with_value_.serialize_with_cv(
        [&expected, &v, &after_set_callback, &result](auto &value, auto &cv) {
          if (expected == value) {
            value = v;
            result = true;
            after_set_callback();
            cv.notify_all();
          }
        });

    return result;
  }

  template <typename Callback = DoNothing>
  ValueType get(const Callback &after_get_callback = DoNothing()) {
    ValueType result;
    monitor_with_value_.serialize_with_cv(
        [&result, &after_get_callback](auto &current_value, auto &) {
          result = current_value;
          after_get_callback();
        });

    return result;
  }

  template <typename Callback = DoNothing>
  void set(const ValueType &v,
           const Callback &after_set_callback = DoNothing()) {
    monitor_with_value_.serialize_with_cv(
        [&v, &after_set_callback](auto &value, auto &cv) {
          value = v;
          after_set_callback();
          cv.notify_all();
        });
  }

  template <typename Callback>
  void change(const Callback &set_callback) {
    monitor_with_value_.serialize_with_cv(
        [&set_callback](auto &value, auto &cv) {
          set_callback(value);
          cv.notify_all();
        });
  }

  template <typename Callback = DoNothing>
  bool is(std::initializer_list<ValueType> expected_values,
          const Callback &after_is_callback = Callback()) {
    bool result{false};
    monitor_with_value_.serialize_with_cv(
        [this, &result, &expected_values, &after_is_callback](
            auto &current_value, auto &) {
          result = helper::container::has(expected_values, current_value);
          if (result) after_is_callback();
        });
    return result;
  }

  template <typename Callback = DoNothing>
  bool is(const ValueType &expected_value,
          const Callback &after_is_callback = Callback()) {
    bool result{false};
    monitor_with_value_.serialize_with_cv(
        [&result, &expected_value, &after_is_callback](auto &current_value,
                                                       auto &) {
          result = (expected_value == current_value);
          if (result) after_is_callback();
        });
    return result;
  }

  template <typename Callback = DoNothing>
  void wait(const ValueType &expected_value,
            const Callback &callback = Callback()) {
    monitor_with_value_.wait(
        [&expected_value, &callback](const auto &current_value) {
          if (expected_value == current_value) {
            callback();
            return true;
          }
          return false;
        });
  }

  template <typename Callback = DoNothing>
  ValueType wait(std::initializer_list<ValueType> expected_values,
                 const Callback &callback = Callback()) {
    ValueType result;
    monitor_with_value_.wait(
        [&expected_values, &callback, &result](const auto &current_value) {
          if (helper::container::has(expected_values, current_value)) {
            result = current_value;
            callback();
            return true;
          }
          return false;
        });

    return result;
  }

  template <class Rep, class Period, typename Callback = DoNothing>
  bool wait_for(const std::chrono::duration<Rep, Period> &rel_time,
                const ValueType &expected_value,
                const Callback &callback = Callback()) {
    return monitor_with_value_.wait_for(
        rel_time, [this, expected_value, &callback](auto &current_value) {
          if (current_value == expected_value) {
            callback();
            return true;
          }
          return false;
        });
  }

  template <class Rep, class Period, typename Callback = DoNothing>
  stdx::expected<ValueType, bool> wait_for(
      const std::chrono::duration<Rep, Period> &rel_time,
      std::initializer_list<ValueType> expected_values,
      const Callback &callback = Callback()) {
    ValueType result;
    if (monitor_with_value_.wait_for(
            rel_time, [this, &expected_values, &result,
                       &callback](const auto &current_value) {
              if (helper::container::has(expected_values, current_value)) {
                result = current_value;
                callback();
                return true;
              }
              return false;
            })) {
      return {result};
    }

    return {stdx::unexpect, true};
  }

 private:
  WaitableMonitor<ValueType> monitor_with_value_{};
};

#endif  // ROUTER_SRC_ROUTER_INCLUDE_HELPER_WAIT_VARIABLE_H_
