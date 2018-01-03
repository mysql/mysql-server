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

#include "plugin/keyring/checker/checker.h"

#include <mysql/psi/mysql_file.h>
#include <memory>

#include "my_compiler.h"

namespace keyring {

const my_off_t Checker::EOF_TAG_SIZE= 3;
const std::string Checker::eofTAG= "EOF";

bool Checker::check_file_structure(File file, size_t file_size, Digest *digest)
{
  if (file_size == 0)
    return is_empty_file_correct(digest) == FALSE;

  return is_file_size_correct(file_size) == FALSE ||
     is_file_tag_correct(file) == FALSE ||
     is_file_version_correct(file) == FALSE ||
     is_dgst_correct(file, digest) == FALSE;
}

bool Checker::is_empty_file_correct(Digest *digest)
{
  return strlen(dummy_digest) == digest->length &&
         strncmp(dummy_digest, reinterpret_cast<const char*>(digest->value),
                 std::min(static_cast<unsigned int>(strlen(dummy_digest)),
                          digest->length)) == 0;
}

bool Checker::is_file_tag_correct(File file)
{
  uchar tag[EOF_TAG_SIZE+1];
  mysql_file_seek(file, 0, MY_SEEK_END, MYF(0));
  if (unlikely(mysql_file_tell(file, MYF(0)) < EOF_TAG_SIZE))
    return FALSE; // File does not contain tag

  if (file_seek_to_tag(file) ||
      unlikely(mysql_file_read(file, tag, EOF_TAG_SIZE, MYF(0)) != EOF_TAG_SIZE))
    return FALSE;
  tag[3]='\0';
  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  return eofTAG == reinterpret_cast<char*>(tag);
}

bool Checker::is_file_version_correct(File file)
{
  std::unique_ptr<uchar[]> version(new uchar[file_version.length()+1]);
  version.get()[file_version.length()]= '\0';
  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  if (unlikely(mysql_file_read(file, version.get(), file_version.length(), MYF(0)) !=
      file_version.length() || file_version != reinterpret_cast<char*>(version.get())))
    return FALSE;

  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  return TRUE;
}

}//namespace keyring

