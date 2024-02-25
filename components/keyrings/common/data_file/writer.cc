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

#include <cstdio>
#include <fstream>

#include "writer.h"

namespace keyring_common {

namespace data_file {
/* Constructor */
File_writer::File_writer(const std::string &file, const std::string &data,
                         bool backup_exists)
    : valid_(true) {
  std::string backup_file(file);
  backup_file.append(".backup");
  /* Do not try to write backup if it already exists */
  if (!backup_exists) valid_ = write_data_to_file(backup_file, data);
  if (valid_)
    valid_ =
        write_data_to_file(file, data) && (remove(backup_file.c_str()) == 0);
}

bool File_writer::write_data_to_file(const std::string &file,
                                     const std::string &data) {
  std::ofstream file_stream(file.c_str());
  if (!file_stream.is_open()) return false;
  bool retval = !(file_stream.write(data.c_str(), data.length())).fail();
  file_stream.close();
  return retval;
}

}  // namespace data_file
}  // namespace keyring_common
