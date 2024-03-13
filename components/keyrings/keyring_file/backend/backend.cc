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

#include <fstream>
#include <memory>

#include <components/keyrings/common/data_file/reader.h>
#include <components/keyrings/common/data_file/writer.h>
#include <components/keyrings/common/json_data/json_reader.h>
#include <components/keyrings/common/json_data/json_writer.h>
#include <components/keyrings/common/memstore/cache.h>
#include <components/keyrings/common/memstore/iterator.h>
#include <components/keyrings/common/utils/utils.h>
#include "backend.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"

namespace keyring_file::backend {

using keyring_common::data::Data;
using keyring_common::data_file::File_reader;
using keyring_common::data_file::File_writer;
using keyring_common::json_data::Json_data_extension;
using keyring_common::json_data::Json_reader;
using keyring_common::json_data::Json_writer;
using keyring_common::json_data::output_vector;
using keyring_common::meta::Metadata;
using keyring_common::utils::get_random_data;

Json_data_extension ext;

Keyring_file_backend::Keyring_file_backend(const std::string &keyring_file_name,
                                           bool read_only)
    : keyring_file_name_(keyring_file_name),
      read_only_(read_only),
      valid_(false) {
  if (keyring_file_name_.length() == 0) {
    LogComponentErr(ERROR_LEVEL, ER_KEYRING_COMPONENT_KEYRING_FILE_NAME_EMPTY);
    return;
  }
  std::string data;
  create_file_if_missing(keyring_file_name_);
  {
    /* Read the file */
    const File_reader file_reader(keyring_file_name_, read_only_, data);
    if (!file_reader.valid()) {
      LogComponentErr(ERROR_LEVEL,
                      ER_KEYRING_COMPONENT_KEYRING_FILE_READ_FAILED,
                      keyring_file_name_.c_str());
      return;
    }
  }

  /* It is possible that file is empty and that's ok. */
  if (data.length()) {
    /* Read JSON data - format check */
    const Json_reader json_reader(data);
    if (!json_reader.valid()) {
      LogComponentErr(ERROR_LEVEL,
                      ER_KEYRING_COMPONENT_KEYRING_FILE_INVALID_FORMAT,
                      keyring_file_name_.c_str());
      return;
    }
    /* Cache */
    json_writer_.set_data(data);
  }
  valid_ = true;
}

bool Keyring_file_backend::load_cache(
    keyring_common::operations::Keyring_operations<Keyring_file_backend>
        &operations) {
  if (json_writer_.num_elements() == 0) return false;
  const Json_reader json_reader(json_writer_.to_string());
  if (!json_reader.valid()) {
    LogComponentErr(ERROR_LEVEL,
                    ER_KEYRING_COMPONENT_KEYRING_FILE_JSON_EXTRACT_FAILED);
    return true;
  }
  if (json_reader.num_elements() != json_writer_.num_elements()) {
    LogComponentErr(ERROR_LEVEL,
                    ER_KEYRING_COMPONENT_KEYRING_FILE_JSON_EXTRACT_FAILED);
    return true;
  }
  for (size_t i = 0; i < json_reader.num_elements(); ++i) {
    std::unique_ptr<Json_data_extension> data_ext;
    Metadata metadata;
    Data data;
    if (json_reader.get_element(i, metadata, data, data_ext)) {
      LogComponentErr(ERROR_LEVEL,
                      ER_KEYRING_COMPONENT_KEYRING_FILE_KEY_EXTRACT_FAILED);
      return true;
    }
    if (operations.insert(metadata, data)) return true;
  }
  return false;
}

bool Keyring_file_backend::get(const Metadata &, Data &) const {
  /* Shouldn't have reached here. */
  return true;
}

bool Keyring_file_backend::store(const Metadata &metadata, Data &data) {
  if (!metadata.valid() || !data.valid()) return true;
  if (json_writer_.add_element(metadata, data, ext)) return true;
  if (write_to_file()) {
    /* Erase stored entry */
    (void)json_writer_.remove_element(metadata, ext);
    return true;
  }
  return false;
}

bool Keyring_file_backend::erase(const Metadata &metadata, Data &data) {
  if (!metadata.valid()) return true;
  if (json_writer_.remove_element(metadata, ext)) return true;
  if (write_to_file()) {
    /* Add entry back */
    (void)json_writer_.add_element(metadata, data, ext);
    return true;
  }
  return false;
}

bool Keyring_file_backend::generate(const Metadata &metadata, Data &data,
                                    size_t length) {
  if (!metadata.valid()) return true;

  const std::unique_ptr<unsigned char[]> key(new unsigned char[length]);
  if (!key) return true;
  if (!get_random_data(key, length)) return true;

  std::string key_str;
  key_str.assign(reinterpret_cast<const char *>(key.get()), length);
  data.set_data(key_str);

  return store(metadata, data);
}

bool Keyring_file_backend::write_to_file() {
  /* Get JSON string from cache and feed it to file writer */
  const File_writer file_writer(keyring_file_name_, json_writer_.to_string());
  return !file_writer.valid();
}

void Keyring_file_backend::create_file_if_missing(
    const std::string &file_name) {
  std::ifstream f(file_name.c_str());
  if (f.good())
    f.close();
  else {
    std::ofstream o(file_name.c_str());
    o.close();
  }
}

}  // namespace keyring_file::backend
