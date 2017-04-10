/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "checker_ver_1_0.h"
#include <mysql/psi/mysql_file.h>

namespace keyring {

bool CheckerVer_1_0::is_file_size_correct(size_t file_size)
{
  return file_size >= ((size_t)EOF_TAG_SIZE + file_version.length());
}
bool CheckerVer_1_0::file_seek_to_tag(File file)
{
  return mysql_file_seek(file, -static_cast<int>(EOF_TAG_SIZE), MY_SEEK_END,
                         MYF(0)) == MY_FILEPOS_ERROR;
}
bool CheckerVer_1_0::is_dgst_correct(File, Digest *digest)
{
  digest->is_empty= TRUE;
  return TRUE;
}

}//namespace keyring
