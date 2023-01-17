/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "plugin/keyring/checker/checker_ver_1_0.h"

#include <mysql/psi/mysql_file.h>

namespace keyring {

bool CheckerVer_1_0::is_file_size_correct(size_t file_size) {
  return file_size >= ((size_t)EOF_TAG_SIZE + file_version.length());
}
bool CheckerVer_1_0::file_seek_to_tag(File file) {
  return mysql_file_seek(file, -static_cast<int>(EOF_TAG_SIZE), MY_SEEK_END,
                         MYF(0)) == MY_FILEPOS_ERROR;
}
bool CheckerVer_1_0::is_dgst_correct(File, Digest *digest) {
  digest->is_empty = true;
  return true;
}

/**
  calculates the size of end-of-file data for particular format
  - it includes fixed size data after the last key in file

  @return size of end-of-file data
*/
size_t CheckerVer_1_0::eof_size() { return EOF_TAG_SIZE; }

}  // namespace keyring
