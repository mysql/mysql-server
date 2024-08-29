/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CONFIG_READER_INCLUDED
#define CONFIG_READER_INCLUDED

#include <string>
#include <vector>

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>

namespace keyring_common::config {

typedef const rapidjson::Value *Config_object;

class Config_reader {
 public:
  /**
    Constructor

    Reads JSON from config file and stores it in memory.

    @param [in] config_file_path Full path to configuration file
  */
  explicit Config_reader(std::string config_file_path);

  /**
    Get an element value from parent element or top level of JSON document.

    @tparam T Type of the element value

    Assumption: Type is compatible with Get() function and
                type of element is matching with template argument.

    @param [in]  element_name  Name of the element being searched
    @param [out] element_value Value of the element
    @param [in]  parent        Parent element, if null top level is being
    searched

    @returns status of search operation
      @retval false Success. Refer to element_value
      @retval true  Failure.
  */
  template <typename T>
  bool get_element(const std::string &element_name, T &element_value,
                   const Config_object &parent = nullptr) {
    if (!valid_) return true;
    if (parent != nullptr) {
      if (!parent->IsObject()) return true;
      return get_element_inner(parent->GetObject(), element_name,
                               element_value);
    }
    return get_element_inner(data_.GetObject(), element_name, element_value);
  }

  /**
    Get an object element from top level of JSON document.

    @param [in]  element_name  Name of the element being searched
    @param [out] element_value Object element

    @returns status of search operation
      @retval false Success. Refer to element_value
      @retval true  Failure.
  */
  bool get_element(const std::string &element_name,
                   Config_object &element_value) {
    if (!valid_ || !data_.HasMember(element_name)) return true;
    element_value = &data_[element_name];
    return false;
  }

  /**
    Get an object element value from parent element of JSON document.

    @param [in]  parent parent element
    @param [in]  element_name  Name of the element being searched
    @param [out] element_value Object element

    @returns status of search operation
      @retval false Success. Refer to element_value
      @retval true  Failure.
  */
  bool get_element(const Config_object &parent, const std::string &element_name,
                   Config_object &element_value) {
    if (!valid_ || parent == nullptr || !parent->IsObject()) return true;
    auto parent_object = parent->GetObject();
    if (!parent_object.HasMember(element_name)) return true;
    element_value = &parent_object[element_name];
    return false;
  }

  /**
    Check if the object is valid, in particular if there was no parse error.

    @param [out] err  when not valid: cause of invalidity

    @returns validity status
      @retval false object not valid, an error occured while creation
      @retval true  object is valid
  */
  bool is_valid(std::string &err) const {
    if (!valid_) err = err_;
    return valid_;
  }

 private:
  /** Configuration file path */
  std::string config_file_path_;
  /** Configuration data in JSON */
  rapidjson::Document data_;
  /** Validity of configuration data */
  bool valid_;
  /** When not valid: cause of invalidity of configuration data */
  std::string err_;

  /**
    Get an element value.

    @tparam P Type of the parent
    @tparam T Type of the element value

    Assumption: Type is compatible with Get() function and
                type of element is matching with template argument.

    @param [in]  parent parent element
    @param [in]  element_name  Name of the element being searched
    @param [out] element_value Value of the element

    @returns status of search operation
      @retval false Success. Refer to element_value
      @retval true  Failure.
  */
  template <typename P, typename T>
  bool get_element_inner(const P &parent, const std::string &element_name,
                         T &element_value) {
    assert(valid_);
    if (!parent.HasMember(element_name)) return true;
    const rapidjson::Value &element = parent[element_name];
    // this check allows avoiding crash if the type is not expected
    if (!element.Is<T>()) return true;
    element_value = element.Get<T>();
    return false;
  }
};

}  // namespace keyring_common::config

#endif  // !CONFIG_READER_INCLUDED
