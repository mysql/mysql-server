/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef MANIFEST_INCLUDED
#define MANIFEST_INCLUDED

#include <fstream> /* std::ifstream */
#include <memory>
#include <string>

#include "scope_guard.h"

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>
#include <rapidjson/schema.h>

namespace manifest {

std::string manifest_version_1_0 =
    "{"
    "  \"title\": \"Manifest validator version 1.0\","
    "  \"description\": \"Expected schema for version 1.0\","
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"read_local_manifest\": {"
    "      \"description\": \"Flag to indicate that manifest information is in "
    "data directory\","
    "      \"type\": \"boolean\""
    "     },"
    "    \"components\": {"
    "      \"description\": \"The list of components to be loaded at "
    "bootstrap\","
    "      \"type\": \"string\""
    "    }"
    "  }"
    "}";

class Manifest_reader final {
 public:
  /*
    Constructor

    Reads manifest file if present.
    Expected format: JSON.

    @param [in] executable_path Executable location
    @param [in] instance_path   Location of specific instance
                                Must have separator character at the end
  */

  explicit Manifest_reader(const std::string executable_path,
                           const std::string instance_path,
                           std::string json_schema = manifest_version_1_0)
      : config_file_path_(),
        schema_(),
        data_(),
        file_present_(false),
        valid_(false),
        empty_(true),
        ro_(true) {
    std::string exe_path(executable_path);
    std::size_t last_separator = exe_path.find_last_of("/\\");
    std::string executable = exe_path.substr(last_separator + 1);
    std::string path = exe_path.erase(last_separator + 1);
#ifdef _WIN32
    std::size_t ext = executable.find(".exe");
    executable = executable.substr(0, ext);
#endif  // _WIN32
    executable.append(".my");
    if (instance_path.length() == 0)
      config_file_path_ = path + executable;
    else
      config_file_path_ = instance_path + executable;
    std::ifstream file_stream(config_file_path_,
                              std::ios::in | std::ios::ate | std::ios::binary);
    if (!file_stream.is_open()) return;
    file_present_ = true;
    {
      /* Check if files read-only or not */
      std::ofstream out_stream(config_file_path_, std::ios_base::app);
      ro_ = !out_stream.is_open();
      out_stream.close();
    }
    auto clean_up = create_scope_guard([&] { file_stream.close(); });
    auto file_length = file_stream.tellg();
    if (file_length > 0) {
      empty_ = false;
      file_stream.seekg(std::ios::beg);
      std::unique_ptr<char[]> read_data(new (std::nothrow) char[file_length]);
      if (!read_data) return;
      if (file_stream.read(read_data.get(), file_length).fail() == true) return;
      std::string data(read_data.get(), file_length);
      if (data_.Parse(data).HasParseError()) return;
      if (schema_.Parse(json_schema).HasParseError()) return;
      {
        rapidjson::Document document;
        if (document.Parse(data).HasParseError()) return;

        rapidjson::SchemaDocument sd(schema_);
        rapidjson::SchemaValidator validator(sd);
        if (!document.Accept(validator)) return;
      }
    }
    valid_ = true;
  }

  ~Manifest_reader() = default;

  bool file_present() const { return file_present_; }
  bool empty() const { return !file_present_ || empty_; }
  bool ro() const { return ro_; }
  std::string manifest_file() const { return config_file_path_; }

  bool read_local_manifest() const {
    bool read_local_manifest = false;
    if (get_element<bool>("read_local_manifest", read_local_manifest) == false)
      return false;
    return read_local_manifest;
  }

  bool components(std::string &components_string) const {
    return get_element<std::string>("components", components_string);
  }

 private:
  /**
    Get an element value from JSON document.
    Assumption: Type is compatible with Get() function and
                type of element is matching with template argument.

    @param [in]  element_name  Name of the element being searched
    @param [out] element_value Value of the element

    @returns status of search operation
      @retval true  Element found. Refer to element_value
      @retval false Element missing.
  */

  template <typename T>
  bool get_element(const std::string element_name, T &element_value) const {
    if (!valid_ || !data_.HasMember(element_name)) return false;
    element_value = data_[element_name].Get<T>();
    return true;
  }

 private:
  /** Configuration file path */
  std::string config_file_path_;
  /** Schema Document */
  rapidjson::Document schema_;
  /** Configuration data in JSON */
  rapidjson::Document data_;
  /** File status */
  bool file_present_;
  /** Validity of configuration data */
  bool valid_;
  /** content */
  bool empty_;
  /** RO flag */
  bool ro_;
};

}  // namespace manifest

#endif  // !MANIFEST_INCLUDED
