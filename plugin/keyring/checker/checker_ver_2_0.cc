/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/keyring/checker/checker_ver_2_0.h"

#include <mysql/psi/mysql_file.h>

#include "my_compiler.h"
#include "plugin/keyring/digest.h"

namespace keyring {

bool CheckerVer_2_0::is_file_size_correct(size_t file_size) {
  return file_size >=
         ((size_t)EOF_TAG_SIZE + file_version.length() + SHA256_DIGEST_LENGTH);
}
bool CheckerVer_2_0::file_seek_to_tag(File file) {
  return mysql_file_seek(file,
                         -static_cast<int>(EOF_TAG_SIZE + SHA256_DIGEST_LENGTH),
                         MY_SEEK_END, MYF(0)) == MY_FILEPOS_ERROR;
}

// Checks if the digest stored in the keyring is the same as the dgst argument
// in case dgst argument is empty it will assign a digest read from the file
//(if it exists) to dgst argument

bool CheckerVer_2_0::is_dgst_correct(File file, Digest *digest) {
  static Digest dgst_read_from_file;

  if (unlikely(mysql_file_seek(file, -SHA256_DIGEST_LENGTH, MY_SEEK_END,
                               MYF(0)) == MY_FILEPOS_ERROR) ||
      mysql_file_read(file, dgst_read_from_file.value, SHA256_DIGEST_LENGTH,
                      MYF(0)) != SHA256_DIGEST_LENGTH)
    return false;
  dgst_read_from_file.is_empty = false;

  if (strncmp(dummy_digest, reinterpret_cast<const char *>(digest->value),
              SHA256_DIGEST_LENGTH) == 0) {
    *digest = dgst_read_from_file;
    return true;
  }
  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  return *digest == dgst_read_from_file;
}

}  // namespace keyring
