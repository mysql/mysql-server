/*
 * Copyright (c) 2021, 2023, Oracle and/or its affiliates.
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

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>

#include "plugin/x/client/xcyclic_buffer.h"

namespace xcl {

class Action {
 public:
  virtual ~Action() = default;

  virtual bool execute(Cyclic_buffer *buffer,
                       std::vector<uint8_t> *all_elements_container) = 0;

  std::vector<uint8_t> create_vector(const uint64_t number_of_elements) {
    return std::vector<uint8_t>(number_of_elements);
  }
};

class Action_insert_linear : public Action {
 public:
  Action_insert_linear(const uint64_t elements_to_insert,
                       const uint64_t start_value, const int32_t step)
      : m_elements_to_insert(elements_to_insert),
        m_start_value(start_value),
        m_step(step),
        m_value(start_value) {}

  bool execute(Cyclic_buffer *buffer,
               std::vector<uint8_t> *all_elements_container) override {
    auto result = create_vector(m_elements_to_insert);

    if (buffer->space_left() < m_elements_to_insert) {
      ADD_FAILURE() << "Cyclic buffer, doesn't have enough free space to put "
                       "data,  expected: "
                    << m_elements_to_insert
                    << ", actual:" << buffer->space_left();
      return false;
    }

    for (auto &element : result) {
      element = (m_value = m_value + m_step);
      all_elements_container->push_back(element);
    }

    buffer->put(result.data(), result.size());

    return true;
  }

  uint64_t m_elements_to_insert;
  uint64_t m_start_value;
  uint32_t m_step;
  uint64_t m_value;
};

class Action_verify : public Action {
 public:
  Action_verify(const uint64_t number_of_elements)
      : m_number_of_elements(number_of_elements) {}

  bool execute(Cyclic_buffer *buffer,
               std::vector<uint8_t> *all_elements_container) override {
    std::vector<uint8_t> result;
    std::vector<uint8_t> matcher;

    if (buffer->space_used() < m_number_of_elements) {
      ADD_FAILURE()
          << "Cyclic buffer, doesn't have enough data to retrieve,  expected: "
          << m_number_of_elements << ", actual:" << buffer->space_used();
      return false;
    }

    result.resize(m_number_of_elements);

    buffer->get(result.data(), result.size());

    std::copy_n(all_elements_container->begin(), m_number_of_elements,
                std::back_inserter(matcher));

    all_elements_container->erase(
        all_elements_container->begin(),
        all_elements_container->begin() + m_number_of_elements);

    if (result.size() != matcher.size()) {
      ADD_FAILURE()
          << "Verify fails because array from cyclic buffer has size: "
          << result.size()
          << ", and it differs from verifying array:" << matcher.size();

      return false;
    }

    for (uint64_t i = 0; i < result.size(); ++i, ++m_element) {
      if (result[i] != matcher[i]) {
        ADD_FAILURE() << "Element at global position: " << m_element
                      << ", local position: " << i
                      << ", differs from matcher array (" << (int)result[i]
                      << " vs " << (int)matcher[i] << ").";
        return false;
      }
    }

    return true;
  }

  uint64_t m_number_of_elements;
  static uint64_t m_element;
};

uint64_t Action_verify::m_element = 0;

class Action_create_cyclic_buffer : public Action {
 public:
  Action_create_cyclic_buffer(const uint64_t number_of_elements)
      : m_number_of_elements(number_of_elements) {}

  bool execute(Cyclic_buffer *buffer, std::vector<uint8_t> *) override {
    buffer->change_size(m_number_of_elements);
    return true;
  }

  uint64_t m_number_of_elements;
};

using Vector_of_actions = std::vector<std::shared_ptr<Action>>;
using Vector_of_bytes = std::vector<uint8_t>;

class Cyclic_buffer_test_suite
    : public testing::TestWithParam<Vector_of_actions> {
 public:
  void SetUp() override { Action_verify::m_element = 0; }

  bool execute_action_base_test(Cyclic_buffer *buffer) {
    Vector_of_bytes matcher;
    const auto param = GetParam();
    int index = 0;

    for (auto action : param) {
      if (!action->execute(buffer, &matcher)) {
        ADD_FAILURE() << "Failed at action " << index;
        return false;
      }
      ++index;
    }

    return true;
  }
};

class Creator {
 public:
  using Action_ptr = std::shared_ptr<Action>;

  static Action_ptr new_cyclic_buffer(const uint64_t numer_of_elements) {
    return Action_ptr(new Action_create_cyclic_buffer(numer_of_elements));
  }

  static Action_ptr insert_linear(const uint64_t numer_of_elements,
                                  const uint64_t start_element,
                                  const uint32_t step) {
    return Action_ptr(
        new Action_insert_linear(numer_of_elements, start_element, step));
  }

  static Action_ptr retrive_and_verify(const uint64_t number_of_element) {
    return Action_ptr(new Action_verify(number_of_element));
  }
};

TEST_P(Cyclic_buffer_test_suite, last_insert_id_not_initialized) {
  Cyclic_buffer buffer(0);

  EXPECT_NO_FATAL_FAILURE(execute_action_base_test(&buffer));
}

INSTANTIATE_TEST_SUITE_P(
    FillsAndRetrives, Cyclic_buffer_test_suite,
    testing::Values(
        // Single fill and single read, without rolling buffer.
        Vector_of_actions({
            Creator::new_cyclic_buffer(1000),
            Creator::insert_linear(1000, 0, 1),
            Creator::retrive_and_verify(1000),
        }),
        // Two fills, second at roll boundry
        Vector_of_actions({
            Creator::new_cyclic_buffer(10),
            Creator::insert_linear(9, 0, 1),
            Creator::retrive_and_verify(1),
            Creator::insert_linear(1, 200, -1),
            Creator::retrive_and_verify(9),
        }),
        // Multiple fills and single retrive, without rolling buffer.
        Vector_of_actions({Creator::new_cyclic_buffer(1000),
                           Creator::insert_linear(200, 0, 1),
                           Creator::insert_linear(200, 200, -1),
                           Creator::insert_linear(100, 0, 2),
                           Creator::insert_linear(100, 200, -2),
                           Creator::insert_linear(200, 200, -1),
                           Creator::insert_linear(200, 0, 1),
                           Creator::retrive_and_verify(1000)}),
        // Multiple fills and matching retrives, without rolling buffer.
        Vector_of_actions({
            Creator::new_cyclic_buffer(1000),
            Creator::insert_linear(200, 0, 1),
            Creator::retrive_and_verify(200),
            Creator::insert_linear(200, 200, -1),
            Creator::retrive_and_verify(200),
            Creator::insert_linear(100, 0, 2),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(100, 200, -2),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(200, 200, -1),
            Creator::retrive_and_verify(200),
            Creator::insert_linear(200, 0, 1),
            Creator::retrive_and_verify(200),
        }),
        // Multiple fills and multiple retrives, without rolling buffer.
        Vector_of_actions({
            Creator::new_cyclic_buffer(1000),
            Creator::insert_linear(200, 0, 1),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(200, 200, -1),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(100, 0, 2),
            Creator::retrive_and_verify(50),
            Creator::insert_linear(100, 200, -2),
            Creator::retrive_and_verify(50),
            Creator::insert_linear(200, 200, -1),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(200, 0, 1),
            Creator::retrive_and_verify(100),
            Creator::retrive_and_verify(500),
        }),
        // Multiple fills and multiple retrives, with rolling buffer.
        Vector_of_actions({
            Creator::new_cyclic_buffer(1000),

            Creator::insert_linear(200, 0, 1),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(200, 200, -1),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(100, 0, 2),
            Creator::retrive_and_verify(50),
            Creator::insert_linear(100, 200, -2),
            Creator::retrive_and_verify(50),
            Creator::insert_linear(200, 200, -1),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(200, 0, 1),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(200, 0, 1),  // First roll
            Creator::retrive_and_verify(100),
            Creator::insert_linear(200, 200, -1),
            Creator::retrive_and_verify(100),
            Creator::insert_linear(100, 0, 2),
            Creator::retrive_and_verify(50),
            Creator::insert_linear(100, 200, -2),
            Creator::retrive_and_verify(50),
            Creator::insert_linear(200, 200, -1),
            Creator::retrive_and_verify(100),
            Creator::retrive_and_verify(900),

        }),
        // Make one roll, and verify the data
        Vector_of_actions({
            Creator::new_cyclic_buffer(1000),
            Creator::insert_linear(1000, 0, 1),
            Creator::retrive_and_verify(400),
            Creator::insert_linear(400, 200, -10),  // roll buffer
            Creator::retrive_and_verify(1000),
        }),
        // Make two rolls, and verify the data
        Vector_of_actions({
            Creator::new_cyclic_buffer(1000),
            Creator::insert_linear(1000, 0, 1),
            Creator::retrive_and_verify(400),
            Creator::insert_linear(400, 200, -10),  // first roll
            Creator::retrive_and_verify(600),
            Creator::insert_linear(600, 0, 20),  // second roll
            Creator::retrive_and_verify(1000),
        })));

}  // namespace xcl
