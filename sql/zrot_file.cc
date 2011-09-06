/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "zgroups.h"


#ifdef HAVE_UGID


Rot_file::Rot_file(char *_file_name)
{
  DBUG_ASSERT(strlen(_file_name) < sizeof(file_name))
  strcpy(file_name, _file_name);
}


int Rot_file::open(bool write)
{
  fd= my_open(file_name, (writable ? O_RDWR | O_CREAT : O_RDONLY) | O_BINARY,
              MYF(MY_WME));
  if (fd < 0)
    DBUG_RETURN(1);

  // check if file is empty
  MY_STAT stat;
  if (my_fstat(ofd, &stat, MYF(MY_WME)) != 0)
    // error doing stat
    goto error_close;
  if (stat.st_size == 0)
    // file has size 0, i.e., is partial
    DBUG_RETURN(rollback());

  DBUG_RETURN(0);
}


int Rot_file::close()
{
  if (my_close(fd, MYF(MY_WME)) == 0)
  {
    fd= 0;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


my_off_t Rot_file::pread(my_off_t offset, my_off_t length, char *buffer)
{
  return my_pread(fd, buffer, length, offset, MYF(MY_WME));
}


void Rot_file::set_rotation_limit(my_off_t limit)
{
}


my_off_t Rot_file::get_rotation_limit(my_off_t limit)
{
}


int Rot_file::purge(my_off_t offset)
{
}


int Rot_file::truncate(my_off_t offset)
{
}


int Rot_file::flush()
{
}


bool is_writable()
{
}


bool is_open()
{
}


~Rot_file()
{
}


