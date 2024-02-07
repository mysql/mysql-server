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

#include <fstream> /* std::ifstream */

#include "reader.h"
#include "writer.h" /* File_writer */

namespace keyring_common {

namespace data_file {
/* Constructor */
File_reader::File_reader(const std::string &file, bool read_only,
                         std::string &data)
    : valid_(false), size_(0) {
  std::string backup_file(file);
  backup_file.append(".backup");
  if (read_data_from_file(backup_file, data)) {
    /*
      Backup file found. Likely because server
      exited in the middle of writing keys to file.
    */

    /*
      We are trying to access keyring in RO mode AND
      we found a valid backup file. So invalidate the
      reader and return.
    */
    if (read_only) return;

    if (data.length() > 0) {
      /* Complete the operation if backup data is valid */
      const File_writer write_from_backup(file, data, true);
      valid_ = write_from_backup.valid();
      if (!valid_) data.clear();
    } else {
      /*
        If backup file was empty, server likely exited
        even before backup file was written to the disk.
        In such a case, do not attempt to restore the
        data because it may overwrite a perfectly good
        keyring data file.
      */
      valid_ = read_data_from_file(file, data);
      remove(backup_file.c_str());
    }
  } else {
    valid_ = read_data_from_file(file, data);
  }
  size_ = data.length();
}

bool File_reader::read_data_from_file(const std::string &file,
                                      std::string &data) {
  bool retval = false;
  std::ifstream file_stream(file, std::ios::in | std::ios::ate);
  if (!file_stream.is_open()) return false;
  auto file_length = file_stream.tellg();
  if (file_length > 0) {
    data.reserve(file_length);
    file_stream.seekg(std::ios::beg);
    char *read_data = new (std::nothrow) char[file_length];
    if (read_data == nullptr) {
      file_stream.close();
      return false;
    }
    retval = !(file_stream.read(read_data, file_length).fail());
    if (retval) data.assign(read_data, file_length);
    delete[] read_data;
  } else {
    /* An empty file is ok. */
    retval = true;
  }
  file_stream.close();
  return retval;
}

}  // namespace data_file

}  // namespace keyring_common
