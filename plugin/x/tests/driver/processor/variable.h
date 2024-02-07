/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_VARIABLE_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_VARIABLE_H_

#include <functional>
#include <string>

class Variable_interface {
 public:
  virtual ~Variable_interface() = default;

  virtual bool set_value(const std::string &value) = 0;
  virtual std::string get_value() const = 0;
};

class Variable_string : public Variable_interface {
 public:
  Variable_string() = default;

  explicit Variable_string(const std::string &value) : m_value(value) {}

  explicit Variable_string(std::string &&value) : m_value(value) {}

  bool set_value(const std::string &value) override {
    m_value = value;

    return true;
  }

  std::string get_value() const override { return m_value; }

 private:
  std::string m_value;
};

class Variable_string_readonly : public Variable_string {
 public:
  explicit Variable_string_readonly(const std::string &value)
      : Variable_string(value) {}

  template <typename Value_type>
  explicit Variable_string_readonly(const Value_type &value)
      : Variable_string(std::to_string(value)) {}

  bool set_value(const std::string &value) override { return false; }
};

class Variable_dynamic_string : public Variable_interface {
 public:
  using String_ref = std::reference_wrapper<std::string>;

  explicit Variable_dynamic_string(const String_ref &value_ref)
      : m_value(value_ref) {}

  bool set_value(const std::string &value) override {
    m_value.get() = value;

    return true;
  }

  std::string get_value() const override { return m_value.get(); }

 private:
  String_ref m_value;
};

class Variable_dynamic_array_of_strings : public Variable_interface {
 public:
  using Array_of_string_ref = std::reference_wrapper<std::vector<std::string>>;

  explicit Variable_dynamic_array_of_strings(
      const Array_of_string_ref &value_ref)
      : m_value(value_ref) {}

  bool set_value(const std::string &value) override { return false; }

  std::string get_value() const override {
    switch (m_value.get().size()) {
      case 0:
        return "";

      case 1:
        return m_value.get()[0];

      default: {
        std::string result = m_value.get()[0];

        for (size_t i = 1; i < m_value.get().size(); ++i) {
          result += "," + m_value.get()[i];
        }

        return result;
      }
    }
  }

 private:
  Array_of_string_ref m_value;
};

class Variable_dynamic_int : public Variable_interface {
 public:
  using Int_ref = std::reference_wrapper<int>;

  explicit Variable_dynamic_int(const Int_ref &value_ref)
      : m_value(value_ref) {}

  bool set_value(const std::string &value) override {
    try {
      m_value.get() = std::stoi(value);
    } catch (const std::exception &) {
      return false;
    }

    return true;
  }

  std::string get_value() const override {
    return std::to_string(m_value.get());
  }

 private:
  Int_ref m_value;
};

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_VARIABLE_H_
