/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PLUGIN_X_CLIENT_VALIDATOR_TRANSLATION_VALIDATOR_H_
#define PLUGIN_X_CLIENT_VALIDATOR_TRANSLATION_VALIDATOR_H_

#include <map>
#include <string>
#include <vector>

#include "my_dbug.h"

#include "plugin/x/client/validator/value_validator.h"
#include "plugin/x/client/visitor/assign_visitor.h"

namespace xcl {

/**
  Validator that translates a string to enum value

  This class checks if Argument_value contains a string object,
  which value is one of "allowed" values and when it stores
  the value, its translated the string to an enum value and
  passed to:

  ```
    virtual void visit_translate(const Translate_to_type value) = 0;
  ```

  Thus uses stores the enum in the callback, not the string.
*/
template <typename Translate_to_type, typename Storage_type,
          bool case_sensitive = true>
class Translate_validator
    : public Value_validator<Storage_type, String_validator> {
 public:
  using Context = Storage_type;
  using Base = Value_validator<Storage_type, String_validator>;
  using Map = std::map<std::string, Translate_to_type>;

 public:
  explicit Translate_validator(const Map &allowed_values)
      : m_allowed_values(update_string_if_case_insensitive(allowed_values)) {}

  virtual void visit_translate(const Translate_to_type value) = 0;

  bool valid_value(const Argument_value &value) override {
    DBUG_TRACE;

    if (0 == m_allowed_values.count(get_string_value(value))) return false;

    return true;
  }

  void store(void *context, const Argument_value &value) override {
    Base::set_ctxt(context);

    visit_translate(m_allowed_values[get_string_value(value)]);
  }

 private:
  std::string get_string_value(const Argument_value &value) const {
    std::string string_value;

    /*
      This class uses String_validator, thus it ensures that
      Argument_value contains string.

      If something other was passed here, this mean its a coding
      fault. "get_value", must always return true.
    */
    if (!get_argument_value(value, &string_value)) {
      assert(false && "Invalid type used in assign visitor");
      return "";
    }

    return update_string_if_case_insensitive(string_value);
  }

  static Map update_string_if_case_insensitive(const Map &map) {
    if (case_sensitive) return map;

    Map result;
    for (const auto &kv : map) {
      result[update_string_if_case_insensitive(kv.first)] = kv.second;
    }

    return result;
  }

  static std::string update_string_if_case_insensitive(
      const std::string &value) {
    if (case_sensitive) return value;
    std::string result;

    result.reserve(value.length() + 1);
    for (const auto c : value) {
      result.push_back(toupper(c));
    }

    return result;
  }

  std::map<std::string, Translate_to_type> m_allowed_values;
};

template <typename Translate_to_type, typename Storage_type,
          bool case_sensitive = true>
class Translate_array_validator
    : public Value_validator<Storage_type, Array_of_strings_validator> {
 public:
  using Base = Value_validator<Storage_type, Array_of_strings_validator>;
  using Array_of_enums = std::vector<Translate_to_type>;
  using Array_of_strings = std::vector<std::string>;
  using Map = std::map<std::string, Translate_to_type>;

 public:
  explicit Translate_array_validator(const Map &allowed_values)
      : m_allowed_values(update_string_if_case_insensitive(allowed_values)) {}

  virtual bool valid_array_value(const Array_of_enums &values) { return true; }
  virtual void visit_translate(const Array_of_enums &values) = 0;
  virtual void visit_translate_with_source(
      const Array_of_enums &enum_values,
      const Array_of_strings &string_values) {
    visit_translate(enum_values);
  }
  virtual bool ignore_unkown_text_values() const { return false; }
  virtual bool ignore_empty_array() const { return false; }

  bool valid_value(const Argument_value &value) override {
    DBUG_TRACE;
    auto values = get_string_values(value);

    Array_of_enums to_additional_verification;

    if (0 == values.size() && !ignore_empty_array()) return false;

    for (const auto &value : values) {
      Translate_to_type enum_value;
      if (!valid_convert_value(value, &enum_value)) {
        DBUG_LOG("debug", "invalid value: \"" << value << "\"");
        if (!ignore_unkown_text_values()) return false;
      }

      to_additional_verification.push_back(enum_value);
    }

    if (!valid_array_value(to_additional_verification)) return false;

    return true;
  }

  void store(void *context, const Argument_value &value) override {
    DBUG_TRACE;
    Base::set_ctxt(context);

    Array_of_enums enum_result;
    Array_of_strings string_result;
    auto values = get_string_values(value);

    for (const auto &value : values) {
      Translate_to_type enum_value;
      if (!valid_convert_value(value, &enum_value)) continue;

      enum_result.push_back(enum_value);
      string_result.push_back(value);
    }

    visit_translate_with_source(enum_result, string_result);
  }

 private:
  bool valid_convert_value(const std::string &value,
                           Translate_to_type *out_value) {
    const auto updated_value = update_string_if_case_insensitive(value);
    if (0 == m_allowed_values.count(updated_value)) {
      return false;
    }

    *out_value = m_allowed_values[updated_value];
    return true;
  }

  Array_of_strings get_string_values(const Argument_value &value) const {
    std::string string_value;

    if (get_argument_value(value, &string_value)) {
      return {string_value};
    }

    Argument_array arguments;
    Array_of_strings result;

    if (get_argument_value(value, &arguments)) {
      for (const auto &arg : arguments) {
        if (get_argument_value(arg, &string_value))
          result.push_back(string_value);
      }
    }

    return result;
  }

  static std::string update_string_if_case_insensitive(
      const std::string &value) {
    if (case_sensitive) return value;

    std::string result;

    result.reserve(value.length() + 1);
    for (const auto c : value) {
      result.push_back(toupper(c));
    }

    return result;
  }

  static Map update_string_if_case_insensitive(const Map &map) {
    if (case_sensitive) return map;

    Map result;
    for (const auto &kv : map) {
      result[update_string_if_case_insensitive(kv.first)] = kv.second;
    }

    return result;
  }

  Map m_allowed_values;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_VALIDATOR_TRANSLATION_VALIDATOR_H_
