/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_KEYRING_STAT_H
#define MYSQL_KEYRING_STAT_H

#include <my_global.h>

namespace keyring
{

typedef MY_STAT My_stat;
struct Keyring_stat : public My_stat
{
  Keyring_stat() : is_initialized(FALSE)
  {
    // Initialize relevant fields in base class.
    st_dev = 0;
    st_ino = 0;
    st_mode = 0;
    st_uid = 0;
    st_gid = 0;
    st_rdev = 0;
    st_size = 0;
    st_mtime = 0;
  }

  my_bool operator==(const MY_STAT& stat)
  {
    return st_dev == stat.st_dev &&
           st_ino == stat.st_ino &&
           st_mode == stat.st_mode &&
           st_uid == stat.st_uid &&
           st_gid == stat.st_gid &&
           st_rdev == stat.st_rdev &&
           st_size == stat.st_size &&
           st_mtime == stat.st_mtime;
  }

  my_bool operator!=(const MY_STAT& stat)
  {
    return !(*this == stat);
  }

  my_bool is_initialized;
};

} //namespace keyring

#endif //MYSQL_KEYRING_STAT_H
