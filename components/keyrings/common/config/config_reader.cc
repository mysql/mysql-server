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

#include <fstream> /* std::ifstream */
#include <memory>

#include "config_reader.h"

#include <rapidjson/istreamwrapper.h> /* IStreamWrapper */

namespace keyring_common {
namespace config {

Config_reader::Config_reader(const std::string config_file_path)
    : config_file_path_(config_file_path), data_(), valid_(false) {
  std::ifstream file_stream(config_file_path_);
  if (!file_stream.is_open()) return;
  rapidjson::IStreamWrapper json_fstream_reader(file_stream);
  valid_ = !data_.ParseStream(json_fstream_reader).HasParseError();
  file_stream.close();
}

}  // namespace config
}  // namespace keyring_common
